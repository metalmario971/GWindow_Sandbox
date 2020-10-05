/**
*  @file VulkanDebug.h
*  @date 20201004
*  @author MetalMario971
*/
#pragma once
#ifndef __VULKANDEBUG_16018075671561130827_H__
#define __VULKANDEBUG_16018075671561130827_H__

#include "./SandboxHeader.h"

namespace VG {


/**
*  @class VulkanDebug
*  @brief A lot of vulkan errors are simple access violations relevant to a struct.
*         Outputs memory offsets of vulkan structs for debugging.
*         Provides enum stringification.
*/
class VulkanDebug {
public:
  static string_t VkGraphicsPipelineCreateInfo_toString();
  static string_t VkResult_toString(VkResult r) ;
  static string_t VkColorSpaceKHR_toString(VkColorSpaceKHR sp);
  static string_t VkFormat_toString(VkFormat fmt);
  //void printQueueFamilyInfo(std::vector<VkQueueFamilyProperties>& fams);

};

}//ns Game



#endif
