#include "./SDLVulkan.h"

/*
  Queue family - a queue with a common set of characteristics, and a number of possible queues.
  Logical device - the features of the physical device that we are using.
    Multiple per phsical device.  

  initialize, SURFACE, create validation layers, pick physical device, create logical device from physical, get surface format from our surface,
   make swapchain, make swapchain images, make image views for images,
   create shader modules
   create pipeline


  opaque pointer
  opaque handle - 
  handle - abstarct refernce to some underlying implementation 
  subpass - render pass that depends ont he contents of the framebuffers of previous passes.

  get surface formats

  Vulkan
  VulkanDevice{
      queue famililes
      deviceproperties
      devicefeatures
      device info
      Context = createLogicalDevice() (create context .. ?)
  }

  Wikipedia
  People whom are experts on subjets get the final say on definitions and terminology in writing. 
  Thsu they should be allowed to write freely on wikipedia.
  They get the final say because of hteir success, thier success being due to some underlying advancement in the organization of the
  knowledge that those whom are not do not possess.

*/

//Macros
//Validate a Vk Result.
#define CheckVKR(fname_, emsg_, ...)                 \
  do {                                               \
    VkResult res__ = (fname_(__VA_ARGS__));          \
    validateVkResult(res__, (Stz emsg_), (#fname_)); \
  } while (0)
//Find Vk Extension
#define VkExtFn(_vkFn) PFN_##_vkFn _vkFn = nullptr;

namespace VG {

class SDLVulkan_Internal {
public:
  SDL_Window* _pSDLWindow = nullptr;
  // std::vector<VkLayerProperties> _availableLayers;
  bool _bEnableValidationLayers = true;

  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> computeFamily;
  std::optional<uint32_t> presentFamily;

  VkSurfaceKHR _main_window_surface = VK_NULL_HANDLE;
  VkInstance _instance = VK_NULL_HANDLE;
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkDevice _device = VK_NULL_HANDLE;
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;

  VkExtent2D swapChainExtent;
  VkFormat swapChainImageFormat;
  std::vector<VkImage> _swapChainImages;
  std::vector<VkImageView> _swapChainImageViews;

  VkQueue graphicsQueue;  // Device queues are implicitly cleaned up when the device is destroyed, so we don't need to do anything in cleanup.
  VkQueue presentQueue;

  VkPipelineLayout _pipelineLayout;


  //Extension functions
  VkExtFn(vkCreateDebugUtilsMessengerEXT);
  // PFN_vkCreateDebugUtilsMessengerEXT
  // vkCreateDebugUtilsMessengerEXT;
  VkExtFn(vkDestroyDebugUtilsMessengerEXT);

  //Error Handling
  void checkErrors() {
    SDLUtils::checkSDLErr();
  }
  void validateVkResult(VkResult res, const string_t& errmsg, const string_t& fname) {
    checkErrors();
    if (res != VK_SUCCESS) {
      errorExit(Stz "Error: '" + fname + "' returned '" + res + "'." + Os::newline() + "  Msg: " + errmsg);
    }
  }
  void exitApp(const std::string& st, int e = -1) {
    std::cout << "Error: " << st << std::endl;
  }
  void errorExit(const string_t& str) {
    SDLUtils::checkSDLErr();
    BRLogError(str);

    Gu::debugBreak();

    BRThrowException(str);
  }

  //Main stuff.

  SDL_Window* makeSDLWindow(const GraphicsWindowCreateParameters& params,
                            int render_system, bool show) {
    string_t title;
    bool bFullscreen = false;
    SDL_Window* ret = nullptr;

    int style_flags = 0;
    style_flags |= (show ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN);
    if (params._type == GraphicsWindowCreateParameters::Wintype_Desktop) {
      style_flags |= SDL_WINDOW_RESIZABLE;
    }
    else if (params._type == GraphicsWindowCreateParameters::Wintype_Utility) {
    }
    else if (params._type == GraphicsWindowCreateParameters::Wintype_Noborder) {
      style_flags |= SDL_WINDOW_BORDERLESS;
    }

    int x = params._x;
    int y = params._y;
    int w = params._width;
    int h = params._height;

    int flags = 0;
#ifdef BR2_OS_IPHONE
    int flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN |
                SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL;
    title = "";
#elif defined(BR2_OS_WINDOWS) || defined(BR2_OS_LINUX)
    flags |= style_flags;
    flags |= render_system;
    title = params._title;
#else
    OS_NOT_SUPPORTED_ERROR
#endif

    // Note: This calls SDL_GL_LOADLIBRARY if SDL_WINDOW_OPENGL is set.
    ret = SDL_CreateWindow(title.c_str(), x, y, w, h, flags);
    if (ret != nullptr) {
      // On Linux SDL will set an error if unable to represent a GL/Vulkan
      // profile, as we try different ones. Ignore them for now. Windows SDL
      // sets an error when the context is created.
      SDLUtils::checkSDLErr();

      // Fullscreen nonsense
      if (bFullscreen) {
        SDL_SetWindowFullscreen(ret, SDL_WINDOW_FULLSCREEN);
      }
      if (show) {
        SDL_ShowWindow(ret);
      }

      SDLUtils::checkSDLErr();
    }
    else {
      // Linux: Couldn't find matching GLX visual.
      SDLUtils::checkSDLErr(true, false);
    }

    // Customize window (per display system)
    // setWindowProps(ret, params);

    checkErrors();

    return ret;
  }

  void init() {
    // Make the window.s
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1) {
      exitApp(Stz "SDL could not be initialized: " + SDL_GetError(), -1);
    }

    string_t title = "hello vuklkan";
    GraphicsWindowCreateParameters params(
        title, 100, 100, 500, 500,
        GraphicsWindowCreateParameters::Wintype_Desktop, false, true, false,
        nullptr);
    _pSDLWindow = makeSDLWindow(params, SDL_WINDOW_VULKAN, false);

    printVideoDiagnostics();

    createVulkanInstance(title, _pSDLWindow);
    loadExtensions();
    setupDebug();
    pickPhysicalDevice();
    createLogicalDevice();
    makeSwapChain();
    makeImageViews();
    createGraphicsPipeline();
  }
  void printGpuSpecs(const VkPhysicalDeviceFeatures* const features) const {
    // features.
  }

  void loadExtensions() {
    // Quick macro.
#define VkLoadExt(_i, _v)                          \
  do {                                             \
    _v = (PFN_##_v)vkGetInstanceProcAddr(_i, #_v); \
    if (_v == nullptr) {                           \
      BRLogError("Could not find " + #_v);         \
    }                                              \
  } while (0)

    // Load Extensions
    VkLoadExt(_instance, vkCreateDebugUtilsMessengerEXT);
    VkLoadExt(_instance, vkDestroyDebugUtilsMessengerEXT);
  }
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

  VkDebugUtilsMessengerCreateInfoEXT populateDebugMessangerCreateInfo() {
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.flags = 0;
    debugCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;
    debugCreateInfo.pUserData = nullptr;
    return debugCreateInfo;
  }
  void setupDebug() {
    if (!_bEnableValidationLayers) {
      return;
    }
    CheckVKR(vkCreateDebugUtilsMessengerEXT, "", _instance, &debugCreateInfo, nullptr, &debugMessenger);
  }

  std::vector<const char*> getValidationLayers() {
    std::vector<const char*> layerNames{};
    if (_bEnableValidationLayers) {
      layerNames.push_back("VK_LAYER_LUNARG_standard_validation");
    }

    //Check if validation layers are supported.
    string_t str = "";
    for (auto layer : layerNames) {
      if (!isValidationLayerSupported(layer)) {
        str += Stz layer + "\r\n";
      }
    }
    if (str.length()) {
      errorExit("One or more validation layers are not supported:\r\n" + str);
    }
    str = "Enabling Validation Layers: \r\n";
    for (auto layer : layerNames) {
      str += Stz "  " + layer;
    }
    BRLogInfo(str);

    return layerNames;
  }
  std::unordered_map<std::string, VkLayerProperties> supported_validation_layers;
  bool isValidationLayerSupported(const string_t& name) {
    if (supported_validation_layers.size() == 0) {
      std::vector<string_t> names;
      uint32_t layerCount;

      CheckVKR(vkEnumerateInstanceLayerProperties, "", &layerCount, nullptr);
      std::vector<VkLayerProperties> availableLayers(layerCount);
      CheckVKR(vkEnumerateInstanceLayerProperties, "", &layerCount, availableLayers.data());

      for (auto layer : availableLayers) {
        supported_validation_layers.insert(std::make_pair(layer.layerName, layer));
      }
    }

    if (supported_validation_layers.find(name) != supported_validation_layers.end()) {
      return true;
    }

    return false;
  }
  std::vector<const char*> getRequiredExtensionNames(SDL_Window* win) {
    std::vector<const char*> extensionNames{};
    //TODO: SDL_Vulkan_GetInstanceExtensions -the window parameter may not need to be valid in future releases.
    //**test this.
    //Returns # of REQUIRED instance extensions
    unsigned int extensionCount;
    if (!SDL_Vulkan_GetInstanceExtensions(win, &extensionCount, nullptr)) {
      errorExit("Couldn't get instance extensions");
    }
    extensionNames = std::vector<const char*>(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(win, &extensionCount,
                                          extensionNames.data())) {
      errorExit("Couldn't get instance extensions (2)");
    }

    // Debug print the extension names.
    std::string exts = "";
    std::string del = "";
    for (const char* st : extensionNames) {
      exts += del + std::string(st) + "\r\n";
      del = "  ";
    }
    BRLogInfo("Available Vulkan Extensions: \r\n" + exts);

    if (_bEnableValidationLayers) {
      extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensionNames;
  }

  void createVulkanInstance(string_t title, SDL_Window* win) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = title.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createinfo{};
    createinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createinfo.pApplicationInfo = &appInfo;

    //Validation layers
    std::vector<const char*> layerNames = getValidationLayers();
    if (_bEnableValidationLayers) {
      createinfo.enabledLayerCount = layerNames.size();
      createinfo.ppEnabledLayerNames = layerNames.data();
    }
    else {
      createinfo.enabledLayerCount = 0;
      createinfo.ppEnabledLayerNames = nullptr;
    }

    //Extensions
    std::vector<const char*> extensionNames = getRequiredExtensionNames(win);
    createinfo.enabledExtensionCount = extensionNames.size();
    createinfo.ppEnabledExtensionNames = extensionNames.data();

    populateDebugMessangerCreateInfo();
    createinfo.pNext = nullptr;  //(VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    createinfo.flags = 0;

    CheckVKR(vkCreateInstance, "Failed to create vulkan instance.", &createinfo, nullptr, &_instance);

    if (!SDL_Vulkan_CreateSurface(win, _instance, &_main_window_surface)) {
      checkErrors();
      errorExit("SDL failed to create vulkan window.");
    }

    debugPrintSupportedExtensions();

    // You can log every vulkan call to stdout.
  }
  void debugPrintSupportedExtensions() {
    // Get extension properties.
    uint32_t extensionCount = 0;
    CheckVKR(vkEnumerateInstanceExtensionProperties, "", nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVKR(vkEnumerateInstanceExtensionProperties, "", nullptr, &extensionCount, extensions.data());
    string_t st = "Supported Vulkan Extensions:" + Os::newline() +
                  "Version   Extension" + Os::newline();
    for (auto ext : extensions) {
      st += Stz "  [" + std::to_string(ext.specVersion) + "] " + ext.extensionName + Os::newline();
    }
    BRLogInfo(st);
  }
  void printVideoDiagnostics() {
    // Init Video
    // SDL_Init(SDL_INIT_VIDEO);

    // Drivers (useless in sdl2)
    const char* driver = SDL_GetCurrentVideoDriver();
    if (driver) {
      BRLogInfo("Default Video Driver: " + driver);
    }
    BRLogInfo("Installed Video Drivers: ");
    int idrivers = SDL_GetNumVideoDrivers();
    for (int idriver = 0; idriver < idrivers; ++idriver) {
      driver = SDL_GetVideoDriver(idriver);
      BRLogInfo(" " + driver);
    }

    // Get current display mode of all displays.
    int nDisplays = SDL_GetNumVideoDisplays();
    BRLogInfo(nDisplays + " Displays:");
    for (int idisplay = 0; idisplay < nDisplays; ++idisplay) {
      SDL_DisplayMode current;
      int should_be_zero = SDL_GetCurrentDisplayMode(idisplay, &current);

      if (should_be_zero != 0) {
        // In case of error...
        BRLogInfo("  Could not get display mode for video display #%d: %s" +
                  idisplay);
        SDLUtils::checkSDLErr();
      }
      else {
        // On success, print the current display mode.
        BRLogInfo("  Display " + idisplay + ": " + current.w + "x" + current.h +
                  ", " + current.refresh_rate + "hz");
        SDLUtils::checkSDLErr();
      }
    }
    checkErrors();
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::string msghead = "[GPU]";
    if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
      msghead += Stz "[G]";
    }
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
      msghead += Stz "[V]";
    }
    else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
      msghead += Stz "[P]";
    }
    else {
      msghead += Stz "[?]";
    }

    std::string msg = "";
    if (pCallbackData != nullptr) {
      msg = std::string(pCallbackData->pMessage);
    }

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
      msghead += Stz "[V]";
      msghead += Stz ":";
      BRLogInfo(msghead + msg);
    }
    else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
      msghead += Stz "[I]";
      msghead += Stz ":";
      BRLogInfo(msghead + msg);
    }
    else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      msghead += Stz "[W]";
      msghead += Stz ":";
      BRLogWarn(msghead + msg);
    }
    else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
      msghead += Stz "[E]";
      msghead += Stz ":";
      BRLogError(msghead + msg);
    }
    else {
      msghead += Stz "[?]";
      msghead += Stz ":";
      BRLogWarn(msghead + msg);
    }
    return VK_FALSE;
  }
  void pickPhysicalDevice() {
    //** TODO: some kind of operatino that lets us choose the best device.
    // Or let the user choose the device, right?

    uint32_t deviceCount = 0;
    CheckVKR(vkEnumeratePhysicalDevices, "", _instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      errorExit("No Vulkan enabled GPUs available.");
    }
    BRLogInfo("Found " + deviceCount + " rendering device(s).");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVKR(vkEnumeratePhysicalDevices, "", _instance, &deviceCount, devices.data());

    // List all devices for debug, and also pick one.
    std::string devInfo = "";
    int i = 0;
    for (const auto& device : devices) {
      VkPhysicalDeviceProperties deviceProperties;
      VkPhysicalDeviceFeatures deviceFeatures;
      vkGetPhysicalDeviceProperties(device, &deviceProperties);
      vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

      devInfo += Stz " Device " + i + ": " + deviceProperties.deviceName + "\r\n";
      devInfo += Stz "  Driver Version: " + deviceProperties.driverVersion + "\r\n";
      devInfo += Stz "  API Version: " + deviceProperties.apiVersion + "\r\n";

      //**NOTE** deviceFeatures must be modified in the deviceFeatures in
      if (_physicalDevice == VK_NULL_HANDLE) {
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader && deviceFeatures.fillModeNonSolid) {
          _physicalDevice = device;
        }
      }

      i++;
    }
    BRLogInfo(devInfo);

    if (_physicalDevice == VK_NULL_HANDLE) {
      errorExit("Failed to find a suitable GPU.");
    }
  }

  //This should be on the logical device class.
  std::unordered_map<string_t, VkExtensionProperties> _deviceExtensions;
  std::unordered_map<string_t, VkExtensionProperties>& getDeviceExtensions() {
    if (_deviceExtensions.size() == 0) {
      // Extensions
      std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
      uint32_t extensionCount;
      CheckVKR(vkEnumerateDeviceExtensionProperties, "", _physicalDevice, nullptr, &extensionCount, nullptr);
      std::vector<VkExtensionProperties> availableExtensions(extensionCount);
      CheckVKR(vkEnumerateDeviceExtensionProperties, "", _physicalDevice, nullptr, &extensionCount, availableExtensions.data());
      for (auto ext : availableExtensions) {
        _deviceExtensions.insert(std::make_pair(ext.extensionName, ext));
      }
    }
    return _deviceExtensions;
  }
  bool isExtensionSupported(const string_t& extName) {
    std::string req_ext = std::string(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (getDeviceExtensions().find(extName) != getDeviceExtensions().end()) {
      return true;
    }
    return false;
  }
  //Here we should really do a "best fit" like we do for OpenGL contexts.
  void createLogicalDevice() {
    //**NOTE** deviceFeatures must be modified in the deviceFeatures in
    // isDeviceSuitable
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.geometryShader = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;  // uh.. uh..
                                                //widelines, largepoints

    // Queues
    findQueueFamilies();

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily.value(),
                                              presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo = {};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = queueFamily;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    //Check Device Extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (auto strext : deviceExtensions) {
      if (!isExtensionSupported(strext)) {
        errorExit(Stz "Extension " + strext + " wasn't supported");
      }
    }

    // Logical Device
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Validation layers are Deprecated
    createInfo.enabledLayerCount = 0;

    CheckVKR(vkCreateDevice, "Failed to create logical device.", _physicalDevice, &createInfo, nullptr, &_device);

    // Create queues
    //**0 is the queue index - this should be checke to make sure that it's less than the queue family size.
    vkGetDeviceQueue(_device, graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(_device, presentFamily.value(), 0, &presentQueue);
  }
  void findQueueFamilies() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies.data());

    string_t qf_info = " Device Queue Families" + Os::newline();

    for (int i = 0; i < queueFamilies.size(); ++i) {
      auto& queueFamily = queueFamilies[i];
      // Check for presentation support
      VkBool32 presentSupport = false;
      CheckVKR(vkGetPhysicalDeviceSurfaceSupportKHR, "", _physicalDevice, i, _main_window_surface, &presentSupport);

      if (queueFamily.queueCount > 0 && presentSupport) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          graphicsFamily = i;
        }
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
          computeFamily = i;
        }
        presentFamily = i;
      }

      qf_info += Stz "  Queue " + i + Os::newline();
      qf_info += Stz "   flags:";
#define PRINT_QUEUE_FLAG(x)             \
  if (queueFamily.queueFlags & x > 0) { \
    qf_info += Stz #x + " ";            \
  }
      PRINT_QUEUE_FLAG(VK_QUEUE_GRAPHICS_BIT);
      PRINT_QUEUE_FLAG(VK_QUEUE_COMPUTE_BIT);
      PRINT_QUEUE_FLAG(VK_QUEUE_TRANSFER_BIT);
      PRINT_QUEUE_FLAG(VK_QUEUE_SPARSE_BINDING_BIT);
      PRINT_QUEUE_FLAG(VK_QUEUE_PROTECTED_BIT);
      qf_info += Os::newline();
      qf_info += Stz "   queueCount:" + queueFamily.queueCount + Os::newline();
      qf_info += Stz "   Image Transfer Granularity: width:" + queueFamily.minImageTransferGranularity.width +
                 "height:" + queueFamily.minImageTransferGranularity.height +
                 "depth:" + queueFamily.minImageTransferGranularity.depth + Os::newline();
    }

    qf_info += "  Selected families (-1 = error):" + Os::newline();
    qf_info += Stz "   Graphics:" + graphicsFamily.value_or(-1) + Os::newline();
    qf_info += Stz "   Compute:" + computeFamily.value_or(-1) + Os::newline();
    qf_info += Stz "   Present:" + presentFamily.value_or(-1) + Os::newline();

    BRLogInfo(qf_info);

    if (graphicsFamily.has_value() == false || presentFamily.has_value() == false) {
      errorExit("GPU doesn't contain any suitable queue families.");
    }
  }

  void makeSwapChain() {
    //VkPresentModeKHR;
    //VkSurfaceFormatKHR;
    uint32_t formatCount;
    CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, "", _physicalDevice, _main_window_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount != 0) {
      CheckVKR(vkGetPhysicalDeviceSurfaceFormatsKHR, "", _physicalDevice, _main_window_surface, &formatCount, formats.data());
    }
    // How the surfaces are presented from the swapchain.
    uint32_t presentModeCount;
    CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, "", _physicalDevice, _main_window_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount != 0) {
      CheckVKR(vkGetPhysicalDeviceSurfacePresentModesKHR, "", _physicalDevice, _main_window_surface, &presentModeCount, presentModes.data());
    }
    //This is cool. Directly query the color space
    VkSurfaceFormatKHR surfaceFormat;
    for (const auto& availableFormat : formats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        surfaceFormat = availableFormat;
        break;
      }
    }
    //VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
    // = VK_PRESENT_MODE_FIFO_KHR = vsync
    VkPresentModeKHR presentMode;
    for (const auto& availablePresentMode : presentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        presentMode = availablePresentMode;
        break;
      }
    }

    VkSurfaceCapabilitiesKHR caps;
    CheckVKR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "", _physicalDevice, _main_window_surface, &caps);

    //Image count, double buffer = 2
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
      imageCount = caps.maxImageCount;
    }

    //Extent = Image size
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      swapChainExtent = caps.currentExtent;
    }
    else {
      VkExtent2D actualExtent = {0, 0};
      actualExtent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, actualExtent.width));
      actualExtent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, actualExtent.height));
      swapChainExtent = actualExtent;
    }

    //Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _main_window_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1;  //more than 1 for stereoscopic application
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;  // ** For window resizing.

    CheckVKR(vkCreateSwapchainKHR, "Failed to create swap chain.", _device, &createInfo, nullptr, &_swapChain);
    //Retrieve images and set format.
    CheckVKR(vkGetSwapchainImagesKHR, "Swapchain images returned invalid.", _device, _swapChain, &imageCount, nullptr);
    _swapChainImages.resize(imageCount);
    CheckVKR(vkGetSwapchainImagesKHR, "", _device, _swapChain, &imageCount, _swapChainImages.data());
    swapChainImageFormat = surfaceFormat.format;
  }
  void makeImageViews() {
    for (size_t i = 0; i < _swapChainImages.size(); i++) {
      VkImageViewCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = _swapChainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = swapChainImageFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      CheckVKR(vkCreateImageView, "", _device, &createInfo, nullptr, &_swapChainImageViews[i]);
    }
  }
  std::vector<char> readFile(const std::string& file) {
    std::vector<char> ret;
    std::fstream fs(file, std::ios::in | std::ios::binary);
    if (!fs.good()) {
      errorExit("Could not open shader file " + file);
      return ret;
    }

    fs.seekg(0, std::ios::end);
    auto size = fs.tellg();
    fs.seekg(0, std::ios::beg);
    ret.reserve(size);
    fs.read(ret.data(), size);
    
    fs.close();
    return ret;
  }
  VkShaderModule _vertShaderModule = VK_NULL_HANDLE;
  VkShaderModule _fragShaderModule = VK_NULL_HANDLE;
  void createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    _vertShaderModule = createShaderModule(vertShaderCode);
    _fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.pNext=nullptr;
    vertShaderStageInfo.flags=0;
    vertShaderStageInfo.module = _vertShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo=nullptr;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineShaderStageCreateInfo fragInf{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.pNext=nullptr;
    vertShaderStageInfo.flags=0;
    vertShaderStageInfo.module = _fragShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.pSpecializationInfo=nullptr;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineShaderStageCreateInfo shaderStages[] = {_vertShaderStageInfo, _fragShaderStageInfo};

    //This is basically a glsl attribute specifying a layout identifier
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional 
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapChainExtent.width;
    viewport.height = (float) swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE; // ** Requires GPU feature
    rasterizer.rasterizerDiscardEnable = VK_FALSE;//Disables output to the framebuffer.
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // ** Requires GPU feature
    rasterizer.lineWidth = 1.0f; // ** Requires WideLines
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
    
    //*Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    //VkPipelineDepthStencilStateCreateInfo // depth / stencil - we'll use null here.

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // colorBlendAttachment.blendEnable = VK_FALSE;
    // colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    // colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    // colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    // colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    // colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    // colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional


    //Pipeline dynamic state. - Change states in the pipeline without rebuilding the pipeline.
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_VIEWPORT
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    CheckVKR(vkCreatePipelineLayout, "", _device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);

  }
  VkShaderModule createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;

    CheckVKR(vkCreateShaderModule, "", device, &createInfo, nullptr, &shaderModule);

    return shaderModule;
  }

  void cleanup() {
    // All child objects created using instance must have been destroyed prior
    // to destroying instance
    vkDestroyShaderModule(_vertShaderModule,nullptr);
    vkDestroyShaderModule(_fragShaderModule,nullptr);
    vkDestroyDevice(_device, nullptr);
    if (_bEnableValidationLayers) {
      vkDestroyDebugUtilsMessengerEXT(_instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(_instance, nullptr);
    SDL_DestroyWindow(_pSDLWindow);
  }

};  // namespace VG

//////////////////////

SDLVulkan::SDLVulkan() {
  _pInt = std::make_unique<SDLVulkan_Internal>();
}

SDLVulkan::~SDLVulkan() {
  _pInt->cleanup();
  _pInt = nullptr;
}
void SDLVulkan::init() {
  try {
    _pInt->init();
  }
  catch (...) {
    _pInt->cleanup();
  }
}

}  // namespace VG