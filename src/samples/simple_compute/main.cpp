#include "simple_compute.h"

/*indPhysicalDevice: {
  device 0, name = Intel(R) UHD Graphics 620 (KBL GT2) <-- (selected)
  device 1, name = llvmpipe (LLVM 12.0.0, 256 bits)
}

Time on GPU: 1609.52, with mean value: 1.88849e-07
Time on CPU: 2103.29, with mean value: 1.88849e-07
*/

int main()
{
  constexpr int LENGTH = 100'000'000;
  constexpr int VULKAN_DEVICE_ID = 0;

  std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(LENGTH);
  if(app == nullptr)
  {
    std::cout << "Can't create render of specified type" << std::endl;
    return 1;
  }

  app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);

  app->Execute();

  return 0;
}
