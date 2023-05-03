#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>
#include <random>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  positionMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "position_map",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  normalMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "normal_map",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  albedoMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "albedo_map",
    .format = vk::Format::eR8G8B8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  SSAO = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "raw_ssao",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  blurredSSAO = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .name = "blurred_ssao",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  ssaoSamples = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float4) * m_uniforms.ssaoKernelSize,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "ssao_samples"
  });

  ssaoNoise = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size        = sizeof(float4) * m_uniforms.ssaoNoiseSize * m_uniforms.ssaoNoiseSize,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "ssao_noise"
  });

  m_uboMappedMem = constants.map();

  //// allocate and fill random noise buffer (for rotation of vectors)
  //
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0, 1.0);

  void* ssaoSamplesMappedMem = ssaoSamples.map();
  std::vector<float4> ssaoSamplesVec;

  ssaoSamplesVec.reserve(m_uniforms.ssaoKernelSize);
  for (size_t i = 0; i < m_uniforms.ssaoKernelSize; i++)
  {
    float4 sample = {dis(gen) * 2.f - 1.f, dis(gen) * 2.f - 1.f, dis(gen), 0.0};
    ssaoSamplesVec.push_back(LiteMath::normalize(sample) * dis(gen));
  }
  memcpy(ssaoSamplesMappedMem, ssaoSamplesVec.data(), ssaoSamplesVec.size() * sizeof(float4));
  ssaoSamples.unmap();

  void *ssaoNoiseMappedMem = ssaoNoise.map();
  std::vector<float4> ssaoNoiseVec;
  unsigned int ssaoNoiseSizeSquares = m_uniforms.ssaoNoiseSize * m_uniforms.ssaoNoiseSize;

  ssaoNoiseVec.reserve(ssaoNoiseSizeSquares);
  for (unsigned int i = 0; i < ssaoNoiseSizeSquares; i++)
  {
    float4 noise = { dis(gen) * 2.f - 1.f, dis(gen) * 2.f - 1.f, 0.f, 0.0f };
    ssaoNoiseVec.push_back(noise);
  }
  memcpy(ssaoNoiseMappedMem, ssaoNoiseVec.data(), ssaoNoiseSizeSquares * sizeof(float4));
  ssaoNoise.unmap();

  //// allocate and fill buffer for bluring
  //

  m_gaussianCoefficients.resize(m_blurWinSize);
  float win_rad = (static_cast<float>(m_blurWinSize) - 1.f) / 2.f;
  float sigma = win_rad / 3.f;
  float sigma2 = 2.f * sigma * sigma;

  float sum = 0.f;

  for (uint32_t i = 0; i < m_blurWinSize; i++)
  {
    float delta = static_cast<float>(i) - win_rad;
    m_gaussianCoefficients[i] = std::exp(- delta * delta / sigma2);
    sum += m_gaussianCoefficients[i];
  }

  for (uint32_t i = 0; i < m_blurWinSize; i++)
    m_gaussianCoefficients[i] /= sum;
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;

  //// random object colors
  //
  std::random_device colorRd;
  std::mt19937 colorGen(colorRd());
  std::uniform_real_distribution<float> colorDis(0.0, 1.0);

  for (size_t i = 0; i < m_pScnMgr->InstancesNum(); i++)
  {
    float3 color = {colorDis(colorGen), colorDis(colorGen), colorDis(colorGen)};
    objColors.push_back(color);
  }
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  positionMap.reset();
  normalMap.reset();
  albedoMap.reset();
  SSAO.reset();
  blurredSSAO.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
  ssaoSamples = etna::Buffer();
  ssaoNoise = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pFSQuad = std::make_shared<vk_utils::QuadRenderer>(0,0, 512, 512);
  m_pFSQuad->Create(m_context->getDevice(),
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad.frag.spv",
    vk_utils::RenderTargetInfo2D{
      .size          = VkExtent2D{ m_width, m_height },// this is debug full screen quad
      .format        = m_swapchain.GetFormat(),
      .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,// seems we need LOAD_OP_LOAD if we want to draw quad to part of screen
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
    }
  );
  SetupSimplePipeline();
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("simple_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});

  etna::create_program("prepare_deferred",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/deferred_prepare.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_deferred",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_deferred.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/quad3_vert.vert.spv"});
  etna::create_program("simple_ssao",
    { VK_GRAPHICS_BASIC_ROOT"/resources/shaders/ssao.frag.spv", VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv" });
  etna::create_program("gaussian_blur",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/bluring.comp.spv"});
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2}
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_context->getDevice(), dtypes, 2);
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, shadowMap.getView({}), defaultSampler.get(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);

  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto blendAttachment = vk::PipelineColorBlendAttachmentState
    {
      .blendEnable = false,
      .colorWriteMask = vk::ColorComponentFlagBits::eR
        | vk::ColorComponentFlagBits::eG
        | vk::ColorComponentFlagBits::eB
        | vk::ColorComponentFlagBits::eA
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });

  m_prepareDeferredPipeline = pipelineManager.createGraphicsPipeline("prepare_deferred",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .blendingConfig =
        {
          .attachments = {blendAttachment, blendAttachment, blendAttachment}
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Sfloat, vk::Format::eR8G8B8A8Srgb},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_deferredPipeline = pipelineManager.createGraphicsPipeline("simple_deferred",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = { static_cast<vk::Format>(m_swapchain.GetFormat()) },
        }
    });
  m_ssaoPipeline = pipelineManager.createGraphicsPipeline("simple_ssao",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32Sfloat},
        }
    });
  m_gaussianBlurPipeline = pipelineManager.createComputePipeline("gaussian_blur", {});
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}



/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    auto inst         = m_pScnMgr->GetInstanceInfo(i);
    pushConst2M.model = m_pScnMgr->GetInstanceMatrix(i);
    pushConst2M.objColor = objColors[i];
    // printf("---> %u - (%f, %f, %f)\n", i, objColors[i][0], objColors[i][1], objColors[i][2]);
    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  //// draw scene to shadowmap
  //
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, {2048, 2048}, {}, shadowMap);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
    DrawSceneCmd(a_cmdBuff, m_lightMatrix);
  }

  //// prepare drawing scene to gbuffers
  //
  {
    auto prepareSimpleDeferredInfo = etna::get_shader_program("prepare_deferred");

    auto set = etna::create_descriptor_set(prepareSimpleDeferredInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{positionMap}, {normalMap}, {albedoMap}}, mainViewDepth);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_prepareDeferredPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_prepareDeferredPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj);
  }

  //// calculate SSAO
  //
  {
    auto ssaoInfo = etna::get_shader_program("simple_ssao");
    auto set = etna::create_descriptor_set(ssaoInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, positionMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, normalMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {3, ssaoSamples.genBinding()},
      etna::Binding {4, ssaoNoise.genBinding()}
    });

    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{SSAO}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_ssaoPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
  }

  //// bluring SSAO
  //
  if (m_uniforms.isSsao)
  {
    auto gaussianBlurInfo = etna::get_shader_program("gaussian_blur");
    auto set = etna::create_descriptor_set(gaussianBlurInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, SSAO.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
      etna::Binding {1, blurredSSAO.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
      etna::Binding {2, positionMap.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
    });
    VkDescriptorSet vkSet = set.getVkSet();
    etna::flush_barriers(a_cmdBuff);

    VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_COMPUTE_BIT);
    vkCmdPushConstants(a_cmdBuff, m_gaussianBlurPipeline.getVkPipelineLayout(),
      stageFlags, 0, sizeof(float) * m_blurWinSize, m_gaussianCoefficients.data());

    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_gaussianBlurPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_gaussianBlurPipeline.getVkPipeline());
    vkCmdDispatch(a_cmdBuff, m_width / 32 + 1, m_height / 32 + 1, 1);
  }

  //// resolve deferred shading
  //
  {
    auto resolveGbufferInfo = etna::get_shader_program("simple_deferred");
    auto set = etna::create_descriptor_set(resolveGbufferInfo.getDescriptorLayoutId(0), a_cmdBuff,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {2, positionMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {3, normalMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {4, albedoMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding {5, blurredSSAO.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });
    VkDescriptorSet vkSet = set.getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImage, a_targetImageView}}, {});

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_deferredPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_deferredPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    vkCmdDraw(a_cmdBuff, 4, 1, 0, 0);
  }


  if(m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }

  etna::set_state(a_cmdBuff, a_targetImage, vk::PipelineStageFlagBits2::eBottomOfPipe,
    vk::AccessFlags2(), vk::ImageLayout::ePresentSrcKHR,
    vk::ImageAspectFlagBits::eColor);

  etna::finish_frame(a_cmdBuff);

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
