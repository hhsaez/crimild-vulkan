/*
 * Copyright (c) 2002 - present, H. Hernan Saez
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Crimild.hpp>
#include <Crimild_Vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#if !defined( NDEBUG )
#define CRIMILD_DEBUG true
#endif

#include <set>
#include <fstream>

namespace crimild {

	namespace vulkan {

		/**
		   \todo Move vkEnumerate* code to templates? Maybe using a lambda for the actual function?
		 */
		class VulkanSimulation {
		public:
			void run( void )
			{
				if ( !initWindow() ) {
					return;
				}
				
				initVulkan();
				loop();
				cleanup();
			}

		private:
            static void errorCallback( int error, const char* description )
            {
                std::cerr << "GLFW Error: (" << error << ") " << description << std::endl;
            }

			crimild::Bool initWindow( void)
			{
				glfwInit();
                glfwSetErrorCallback( errorCallback );
				glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
				glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

				_window = glfwCreateWindow( _width, _height, "Triangle", nullptr, nullptr );

				return true;
			}
			
			void initVulkan( void )
			{
				createInstance();
				setupDebugMessenger();
				createSurface();
				pickPhysicalDevice();
				createLogicalDevice();
				createSwapChain();
				createImageViews();
				createGraphicsPipeline();
				createRenderPass();
			}

			void createInstance( void )
			{
#ifdef CRIMILD_DEBUG
				_enableValidationLayers = true;
#endif

				if ( _enableValidationLayers && !checkValidationLayerSupport( _validationLayers ) ) {
					throw RuntimeException( "Validation layers requested, but not available" );
				}

				const auto appInfo = VkApplicationInfo {
					.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
					.pApplicationName = "Triangle",
					.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
					.pEngineName = "Crimild",
					.engineVersion = VK_MAKE_VERSION( CRIMILD_VERSION_MAJOR, CRIMILD_VERSION_MINOR, CRIMILD_VERSION_PATCH ),
					.apiVersion = VK_API_VERSION_1_0,
				};
				
				auto extensions = getRequiredExtensions();
				
				auto createInfo = VkInstanceCreateInfo {
					.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
					.pApplicationInfo = &appInfo,
					.enabledExtensionCount = static_cast< crimild::UInt32 >( extensions.size() ),
					.ppEnabledExtensionNames = extensions.data(),
					.enabledLayerCount = 0,
				};

				if ( _enableValidationLayers ) {
					createInfo.enabledLayerCount = static_cast< crimild::UInt32 >( _validationLayers.size() );
					createInfo.ppEnabledLayerNames = _validationLayers.data();					
				}

				if ( vkCreateInstance( &createInfo, nullptr, &_instance ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create Vulkan instance" );
				}
			}

			std::vector< const char * > getRequiredExtensions( void )
			{
				if ( _enableValidationLayers ) {
					// list all available extensions
					crimild::UInt32 extensionCount = 0;
					vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
					std::vector< VkExtensionProperties > extensions( extensionCount );
					vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );
					std::cout << "Available extensions: ";
					for ( const auto &extension : extensions ) {
						std::cout << "\n\t" << extension.extensionName;
					}
					std::cout << "\n";
				}
				
				crimild::UInt32 glfwExtensionCount = 0;
				const char **glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

				std::vector< const char * > extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );
				std::cout << "Available GLFW extensions: ";
				for ( const auto &extension : extensions ) {
					std::cout << "\n\t" << extension;
				}
				std::cout << "\n";

				if ( _enableValidationLayers ) {
					extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
				}

				return extensions;
			}

			crimild::Bool checkValidationLayerSupport( const std::vector< const char * > &validationLayers )
			{
				crimild::UInt32 layerCount;
                vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

                std::vector< VkLayerProperties > availableLayers( layerCount );
                vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

				for ( const auto layerName : validationLayers ) {
					auto layerFound = false;
					for ( const auto &layerProperites : availableLayers ) {
						if ( strcmp( layerName, layerProperites.layerName ) == 0 ) {
							layerFound = true;
							break;
						}
					}

					if ( !layerFound ) {
						return false;
					}
				}

				return true;
			}

			void setupDebugMessenger( void )
			{
				if ( !_enableValidationLayers ) {
					return;
				}

				VkDebugUtilsMessengerCreateInfoEXT createInfo = {
					.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
					.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
					.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
					.pfnUserCallback = debugCallback,
					.pUserData = nullptr,
				};

				if ( createDebugUtilsMessengerEXT( _instance, &createInfo, nullptr, &_debugMessenger ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to setup debug messenger" );
				}
			}

			/**
			   \name Physical devices
			*/
			//@{

		private:
			void pickPhysicalDevice( void )
			{
				crimild::UInt32 deviceCount = 0;
				vkEnumeratePhysicalDevices( _instance, &deviceCount, nullptr );
				if ( deviceCount == 0 ) {
					throw RuntimeException( "Failed to find GPUs with Vulkan support!" );
				}

				std::vector< VkPhysicalDevice > devices( deviceCount );
				vkEnumeratePhysicalDevices( _instance, &deviceCount, devices.data() );
				for ( const auto &device : devices ) {
					if ( isDeviceSuitable( device ) ) {
						_physicalDevice = device;
						break;
					}
				}

				if ( _physicalDevice == VK_NULL_HANDLE ) {
					throw RuntimeException( "Failed to find a suitable GPU" );
				}
			}

			crimild::Bool isDeviceSuitable( VkPhysicalDevice device ) const noexcept
			{
				auto indices = findQueueFamilies( device );
				auto extensionsSupported = checkDeviceExtensionSupport( device );

				auto swapChainAdequate = false;
				if ( extensionsSupported ) {
					auto swapChainSupport = querySwapChainSupport( device );
					swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
				}
				
				return indices.isComplete() && extensionsSupported && swapChainAdequate;
			}

			/**
			   \brief Check if a given device met all required extensions
			 */
			crimild::Bool checkDeviceExtensionSupport( VkPhysicalDevice device ) const noexcept
			{
				CRIMILD_LOG_DEBUG( "Checking device extension support" );
				
				crimild::UInt32 extensionCount;
				vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );
				
				std::vector< VkExtensionProperties > availableExtensions( extensionCount );
				vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, availableExtensions.data() );

				std::set< std::string > requiredExtensions( std::begin( m_deviceExtensions ), std::end( m_deviceExtensions ) );

				for ( const auto &extension : availableExtensions ) {
					requiredExtensions.erase( extension.extensionName );
				}

				if ( !requiredExtensions.empty() ) {
					std::stringstream ss;
					for ( const auto &name : requiredExtensions ) {
						ss << "\n\t" << name;
					}
					CRIMILD_LOG_ERROR( "Required extensions not met: ", ss.str() );
					return false;
				}

				CRIMILD_LOG_DEBUG( "All required extensions met" );
				
				return true;
			}

		private:
			VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;

			//@}

			/**
			   \name Queues
			*/
			//@{

		private:
			struct QueueFamilyIndices {
				std::vector< crimild::UInt32 > graphicsFamily;
				std::vector< crimild::UInt32 > presentFamily;

				bool isComplete( void )
				{
					return graphicsFamily.size() > 0 && presentFamily.size() > 0;
				}
			};
			
			QueueFamilyIndices findQueueFamilies( VkPhysicalDevice device ) const noexcept
			{
				QueueFamilyIndices indices;
				
				crimild::UInt32 queueFamilyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );
				
				std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
				vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );
				
				crimild::UInt32 i = 0;
				for ( const auto &queueFamily : queueFamilies ) {
					if ( queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
						indices.graphicsFamily.push_back( i );
					}

					VkBool32 presentSupport = false;
					vkGetPhysicalDeviceSurfaceSupportKHR( device, i, _surface, &presentSupport );
					if ( queueFamily.queueCount > 0 && presentSupport ) {
						indices.presentFamily.push_back( i );
					}

					if ( indices.isComplete() ) {
						break;
					}

					i++;
				}

				return indices;
			}

			//@}
			

		private:
			static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
				VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageType,
				const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
				void *pUserData )
			{
				std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
				return VK_FALSE;
			}

			static VkResult createDebugUtilsMessengerEXT(
				VkInstance instance,
				const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
				const VkAllocationCallbacks *pAllocator,
				VkDebugUtilsMessengerEXT *pDebugMessenger )
			{
				auto func = ( PFN_vkCreateDebugUtilsMessengerEXT ) vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
				if ( func != nullptr ) {
					return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
				}
				else {
					return VK_ERROR_EXTENSION_NOT_PRESENT;
				}
			}

			static void destroyDebugUtilsMessengerEXT(
				VkInstance instance,
				VkDebugUtilsMessengerEXT debugMessenger,
				const VkAllocationCallbacks *pAllocator )
			{
				auto func = ( PFN_vkDestroyDebugUtilsMessengerEXT ) vkGetInstanceProcAddr(
					instance,
					"vkDestroyDebugUtilsMessengerEXT" );
				if ( func != nullptr ) {
					func( instance, debugMessenger, pAllocator );
				}
			}

		private:
			void loop( void )
			{
				while ( !glfwWindowShouldClose( _window ) ) {
					glfwPollEvents();
				}
			}

		private:
			GLFWwindow *_window = nullptr;
			crimild::UInt32 _width = 1028;
			crimild::UInt32 _height = 768;
			
			VkInstance _instance;
			crimild::Bool _enableValidationLayers = false;
			const std::vector< const char * > _validationLayers {
				"VK_LAYER_LUNARG_standard_validation",
			};				
			VkDebugUtilsMessengerEXT _debugMessenger;
			const std::vector< const char * > m_deviceExtensions {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			};

			/**
			   \name Logical devices
			*/
			//@{

		private:
			void createLogicalDevice( void )
			{
				QueueFamilyIndices indices = findQueueFamilies( _physicalDevice );
				if ( !indices.isComplete() ) {
					// should never happen
					throw RuntimeException( "Invalid physical device" );
				}

				std::vector< VkDeviceQueueCreateInfo > queueCreateInfos;
				std::set< crimild::UInt32 > uniqueQueueFamilies = {
					indices.graphicsFamily[ 0 ],
					indices.presentFamily[ 0 ],
				};

				// Required even if there's only one queue
				auto queuePriority = 1.0f;

				for ( auto queueFamily : uniqueQueueFamilies ) {
					auto queueCreateInfo = VkDeviceQueueCreateInfo {
						.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
						.queueFamilyIndex = queueFamily,
						.queueCount = 1,
						.pQueuePriorities = &queuePriority,
					};
					queueCreateInfos.push_back( queueCreateInfo );
				}

				// TODO
				VkPhysicalDeviceFeatures deviceFeatures = {};

				VkDeviceCreateInfo createInfo = {
					.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
					.queueCreateInfoCount = static_cast< crimild::UInt32 >( queueCreateInfos.size() ),
					.pQueueCreateInfos = queueCreateInfos.data(),
					.pEnabledFeatures = &deviceFeatures,
					.enabledExtensionCount = static_cast< crimild::UInt32 >( m_deviceExtensions.size() ),
					.ppEnabledExtensionNames = m_deviceExtensions.data(),
				};

				if ( _enableValidationLayers ) {
					createInfo.enabledLayerCount = static_cast< crimild::UInt32 >( _validationLayers.size() );
					createInfo.ppEnabledLayerNames = _validationLayers.data();
				}
				else {
					createInfo.enabledLayerCount = 0;
				}

				if ( vkCreateDevice( _physicalDevice, &createInfo, nullptr, &m_device ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create logical device" );
				}

				// Get queue handles
				vkGetDeviceQueue( m_device, indices.graphicsFamily[ 0 ], 0, &m_graphicsQueue );
				vkGetDeviceQueue( m_device, indices.presentFamily[ 0 ], 0, &m_presentQueue );
			}

		private:
			VkDevice m_device;
			VkQueue m_graphicsQueue;
			VkQueue m_presentQueue;

			//@}

			/**
			   \name Window Surface
			 */
			//@{

		private:
			void createSurface( void )
			{
				auto result = glfwCreateWindowSurface( _instance, _window, nullptr, &_surface );
				if ( result != VK_SUCCESS ) {
					std::stringstream ss;
					ss << "Failed to create window surface. Error: " << result;
					throw RuntimeException( ss.str() );
				}
			}

		private:
			VkSurfaceKHR _surface;

			//@}

			/**
			   \name SwapChain 
			*/
			//@{
		private:
			struct SwapChainSupportDetails {
				VkSurfaceCapabilitiesKHR capabilities;
				std::vector< VkSurfaceFormatKHR > formats;
				std::vector< VkPresentModeKHR > presentModes;
			};

			SwapChainSupportDetails querySwapChainSupport( VkPhysicalDevice device ) const noexcept				
			{
				SwapChainSupportDetails details;

				vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, _surface, &details.capabilities );

				crimild::UInt32 formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR( device, _surface, &formatCount, nullptr );
				if ( formatCount > 0 ) {
					details.formats.resize( formatCount );
					vkGetPhysicalDeviceSurfaceFormatsKHR( device, _surface, &formatCount, details.formats.data() );
				}

				crimild::UInt32 presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR( device, _surface, &presentModeCount, nullptr );
				if ( presentModeCount > 0 ) {
					details.presentModes.resize( presentModeCount );
					vkGetPhysicalDeviceSurfacePresentModesKHR( device, _surface, &presentModeCount, details.presentModes.data() );
				}
				
				return details;
			}

			VkSurfaceFormatKHR chooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR > &availableFormats ) const noexcept
			{
				if ( availableFormats.size() == 1 && availableFormats[ 0 ].format == VK_FORMAT_UNDEFINED ) {
					return {
						VK_FORMAT_B8G8R8A8_UNORM,
						VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
					};
				}

				for ( const auto &availableFormat : availableFormats ) {
					if ( availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
						return availableFormat;
					}
				}

				return availableFormats[ 0 ];
			}

			VkPresentModeKHR chooseSwapPresentMode( const std::vector< VkPresentModeKHR > &availablePresentModes ) const noexcept
			{
				VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
				
				for ( const auto &availablePresentMode : availablePresentModes ) {
					if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR ) {
						// Triple buffer
						return availablePresentMode;
					}
					else if ( availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ) {
						// Double buffer
						bestMode = availablePresentMode;
					}
				}
				
				return bestMode;
			}

			VkExtent2D chooseSwapExtent( const VkSurfaceCapabilitiesKHR capabilities )
			{
				if ( capabilities.currentExtent.width != std::numeric_limits< uint32_t >::max() ) {
					return capabilities.currentExtent;
				}

				VkExtent2D actualExtent = {
					_width,
					_height,
				};

				actualExtent.width = std::max(
					capabilities.minImageExtent.width,
					std::min( capabilities.maxImageExtent.width, actualExtent.width )
				);
				actualExtent.height = std::max(
					capabilities.minImageExtent.height,
					std::min( capabilities.maxImageExtent.height, actualExtent.height )
				);

				return actualExtent;
			}

			void createSwapChain( void )
			{
				CRIMILD_LOG_DEBUG( "Creating swapchain" );
				
				auto swapChainSupport = querySwapChainSupport( _physicalDevice );
				auto surfaceFormat = chooseSwapSurfaceFormat( swapChainSupport.formats );
				auto presentMode = chooseSwapPresentMode( swapChainSupport.presentModes );
				auto extent = chooseSwapExtent( swapChainSupport.capabilities );

				auto imageCount = swapChainSupport.capabilities.minImageCount + 1;
				if ( swapChainSupport.capabilities.maxImageCount > 0 ) {
					imageCount = std::min(
						swapChainSupport.capabilities.maxImageCount,
						imageCount
					);
				}

				auto createInfo = VkSwapchainCreateInfoKHR {
					.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
					.surface = _surface,
					.minImageCount = imageCount,
					.imageFormat = surfaceFormat.format,
					.imageColorSpace = surfaceFormat.colorSpace,
					.imageExtent = extent,
					.imageArrayLayers = 1,
					.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
				};

				auto indices = findQueueFamilies( _physicalDevice );
				uint32_t queueFamilyIndices[] = {
					indices.graphicsFamily[ 0 ],
					indices.presentFamily[ 0 ],
				};

				if ( indices.graphicsFamily != indices.presentFamily ) {
					createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
					createInfo.queueFamilyIndexCount = 2;
					createInfo.pQueueFamilyIndices = queueFamilyIndices;
				}
				else {
					createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
					createInfo.queueFamilyIndexCount = 0;
					createInfo.pQueueFamilyIndices = nullptr;
				}

				createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
				createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				createInfo.presentMode = presentMode;
				createInfo.clipped = VK_TRUE;
				createInfo.oldSwapchain = VK_NULL_HANDLE;

				if ( vkCreateSwapchainKHR( m_device, &createInfo, nullptr, &m_swapChain ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create swapchain" );
				}

				vkGetSwapchainImagesKHR( m_device, m_swapChain, &imageCount, nullptr );
				m_swapChainImages.resize( imageCount );
				vkGetSwapchainImagesKHR( m_device, m_swapChain, &imageCount, m_swapChainImages.data() );

				m_swapChainImageFormat = surfaceFormat.format;
				m_swapChainExtent = extent;
			}

		private:
			VkSwapchainKHR m_swapChain;
			std::vector< VkImage > m_swapChainImages;
			VkFormat m_swapChainImageFormat;
			VkExtent2D m_swapChainExtent;

			//@}

			/**
			   \name Image views
			*/
			//@{

		public:
			void createImageViews( void )
			{
				m_swapChainImageViews.resize( m_swapChainImages.size() );
				
				for ( auto i = 0l; i < m_swapChainImages.size(); ++i ) {
					auto createInfo = VkImageViewCreateInfo {
						.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
						.image = m_swapChainImages[ i ],
						.viewType = VK_IMAGE_VIEW_TYPE_2D,
						.format = m_swapChainImageFormat,
						.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
						.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
						.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
						.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
						.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.subresourceRange.baseMipLevel = 0,
						.subresourceRange.levelCount = 1,
						.subresourceRange.baseArrayLayer = 0,
						.subresourceRange.layerCount = 1,
					};

					if ( vkCreateImageView( m_device, &createInfo, nullptr, &m_swapChainImageViews[ i ] ) != VK_SUCCESS ) {
						throw RuntimeException( "Failed to create image views" );
					}
				}
			}

		private:
			std::vector< VkImageView > m_swapChainImageViews;

			//@}

			/**
			   \name Graphics Pipeline
			*/
			//@{
			
		public:
			void createGraphicsPipeline( void )
			{
				auto vertShaderCode = readFile( "shaders/vert.spv" );
				auto fragShaderCode = readFile( "shaders/frag.spv" );

				// Shader Modules
				auto vertShaderModule = createShaderModule( vertShaderCode );
				auto fragShaderModule = createShaderModule( fragShaderCode );

				// Shader Stage Creation
				auto vertShaderStageInfo = VkPipelineShaderStageCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.module = vertShaderModule,
					.pName = "main",
				};

				auto fragShaderStageInfo = VkPipelineShaderStageCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.module = fragShaderModule,
					.pName = "main",
				};

				VkPipelineShaderStageCreateInfo shaderStages[] = {
					vertShaderStageInfo,
					fragShaderStageInfo,
				};

				// Vertex Input

				auto vertexInputInfo = VkPipelineVertexInputStateCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
					.vertexBindingDescriptionCount = 0,
					.pVertexBindingDescriptions = nullptr,
					.vertexAttributeDescriptionCount = 0,
					.pVertexAttributeDescriptions = nullptr,
				};

				// Input Assembly

				auto inputAssemby = VkPipelineInputAssemblyStateCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
					.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
					.primitiveRestartEnable = VK_FALSE,
				};

				// Viewports and scissors

				auto viewport = VkViewport {
					.x = 0.0f,
					.y = 0.0f,
					.width = ( float ) m_swapChainExtent.width,
					.height = ( float ) m_swapChainExtent.height,
					.minDepth = 0.0f,
					.maxDepth = 1.0f,
				};

				auto scissor = VkRect2D {
					.offset = { 0, 0 },
					.extent = m_swapChainExtent,
				};

				auto viewportState = VkPipelineViewportStateCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
					.viewportCount = 1,
					.pViewports = &viewport,
					.scissorCount = 1,
					.pScissors = &scissor,
				};

				// Rasterizer

				auto rasterizer = VkPipelineRasterizationStateCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
					.depthClampEnable = VK_FALSE, // VK_TRUE might be required for shadow maps
					.rasterizerDiscardEnable = VK_FALSE, // VK_TRUE disables output to the framebuffer
					.polygonMode = VK_POLYGON_MODE_FILL,
					.lineWidth = 1.0f,
					.cullMode = VK_CULL_MODE_BACK_BIT,
					.frontFace = VK_FRONT_FACE_CLOCKWISE,
					.depthBiasEnable = VK_FALSE, // Might be needed for shadow mapping
					.depthBiasConstantFactor = 0.0f,
					.depthBiasClamp = 0.0f,
					.depthBiasSlopeFactor = 0.0f,
				};

				// Multisampling

				auto multisampling = VkPipelineMultisampleStateCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
					.sampleShadingEnable = VK_FALSE,
					.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
					.minSampleShading = 1.0f,
					.pSampleMask = nullptr,
					.alphaToCoverageEnable = VK_FALSE,
					.alphaToOneEnable = VK_FALSE,
				};

				// Depth and Stencil testing (TBD)

				// Color Blending

				auto colorBlendAttachment = VkPipelineColorBlendAttachmentState {
					.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
					.blendEnable = VK_FALSE,
					.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
					.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
					.colorBlendOp = VK_BLEND_OP_ADD,
					.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
					.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
					.alphaBlendOp = VK_BLEND_OP_ADD,
				};

				auto colorBlending = VkPipelineColorBlendStateCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
					.logicOpEnable = VK_FALSE,
					.logicOp = VK_LOGIC_OP_COPY,
					.attachmentCount = 1,
					.pAttachments = &colorBlendAttachment,
					.blendConstants[ 0 ] = 0.0f,
					.blendConstants[ 1 ] = 0.0f,
					.blendConstants[ 2 ] = 0.0f,
					.blendConstants[ 3 ] = 0.0f,
				};

				// Dynamic State (TBD)

				// Pipeline layout

				auto pipelineLayoutInfo = VkPipelineLayoutCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
					.setLayoutCount = 0,
					.pSetLayouts = nullptr,
					.pushConstantRangeCount = 0,
					.pPushConstantRanges = nullptr,
				};

				if ( vkCreatePipelineLayout( m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create pipeline layout" );
				}

				// Cleanup
				vkDestroyShaderModule( m_device, fragShaderModule, nullptr );
				vkDestroyShaderModule( m_device, vertShaderModule, nullptr );
			}

		private:
			VkPipelineLayout m_pipelineLayout;

			//@}

			/**
			   \name Shaders
			*/
			//@{

		private:

			static std::vector< char > readFile( const std::string &filename )
			{
				std::ifstream file( filename, std::ios::ate | std::ios::binary );
				if ( !file.is_open() ) {
					throw FileNotFoundException( "Failed to open file: " + filename );
				}

				auto fileSize = ( size_t ) file.tellg();
				std::vector< char > buffer( fileSize );

				file.seekg( 0 );
				file.read( buffer.data(), fileSize );

				file.close();

				CRIMILD_LOG_DEBUG( "File ", filename, " loaded (", fileSize , " bytes)" );

				return buffer;
			}

			VkShaderModule createShaderModule( const std::vector< char > &code )
			{
				auto createInfo = VkShaderModuleCreateInfo {
					.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					.codeSize = code.size(),
					.pCode = reinterpret_cast< const uint32_t * >( code.data() ),
				};

				VkShaderModule shaderModule;
				if ( vkCreateShaderModule( m_device, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create shader module" );
				}

				return shaderModule;
			}

			//@}

			/**
			   \name Render Passes
			*/
			//@{

		private:

			void createRenderPass( void )
			{
				// Attachment description
				
				auto colorAttachment = VkAttachmentDescription {
					.format = m_swapChainImageFormat,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				};

				// Subpasses and attachment references

				auto colorAttachmentRef = VkAttachmentReference {
					.attachment = 0,
					.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				};

				auto subpass = VkSubpassDescription {
					.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
					.colorAttachmentCount = 1,
					.pColorAttachments = &colorAttachmentRef,
				};

				// Render Pass

				auto renderPassInfo = VkRenderPassCreateInfo {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
					.attachmentCount = 1,
					.pAttachments = &colorAttachment,
					.subpassCount = 1,
					.pSubpasses = &subpass,
				};

				if ( vkCreateRenderPass( m_device, &renderPassInfo, nullptr, &m_renderPass ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create render pass" );
				}
			}

		private:
			VkRenderPass m_renderPass;

			//@}
			

			/**
			   \name Cleanup
			*/
			//@{
			
		private:
			void cleanup( void )
			{
				// TODO: The order of these calls is causing a SEGFAULT

				vkDestroyPipelineLayout( m_device, m_pipelineLayout, nullptr );
				vkDestroyRenderPass( m_device, m_renderPass, nullptr );
				
				for ( auto imageView : m_swapChainImageViews ) {
					vkDestroyImageView( m_device, imageView, nullptr );
				}
				
				vkDestroySwapchainKHR( m_device, m_swapChain, nullptr );
				
				vkDestroyDevice( m_device, nullptr );
				
				if ( _enableValidationLayers ) {
					destroyDebugUtilsMessengerEXT( _instance, _debugMessenger, nullptr );
				}

				vkDestroySurfaceKHR( _instance, _surface, nullptr );
				
				vkDestroyInstance( _instance, nullptr );
				
				if ( _window != nullptr ) {
					glfwDestroyWindow( _window );
					_window = nullptr;
				}

				glfwTerminate();
			}

			//@}

		};

	}

}

using namespace crimild;
using namespace vulkan;

int main( int argc, char **argv )
{
	VulkanSimulation sim;
	sim.run();
	
	return 0;
}


/*
int main( int argc, char **argv )
{
	crimild::init();
	
    CRIMILD_SIMULATION_LIFETIME auto sim = crimild::alloc< sdl::SDLSimulation >( "Triangle", crimild::alloc< Settings >( argc, argv ) );

    auto scene = crimild::alloc< Group >();

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f
    };

    unsigned short indices[] = {
        0, 1, 2
    };

    auto primitive = crimild::alloc< Primitive >();
    primitive->setVertexBuffer( crimild::alloc< VertexBufferObject >( VertexFormat::VF_P3_C4, 3, vertices ) );
    primitive->setIndexBuffer( crimild::alloc< IndexBufferObject >( 3, indices ) );

    auto geometry = crimild::alloc< Geometry >();
    geometry->attachPrimitive( primitive );
    auto material = crimild::alloc< Material >();
	material->setDiffuse( RGBAColorf( 0.0f, 1.0f, 0.0f, 1.0f ) );
	material->setProgram( crimild::alloc< VertexColorShaderProgram >() );
    material->getCullFaceState()->setEnabled( false );
    geometry->getComponent< MaterialComponent >()->attachMaterial( material );
    geometry->attachComponent< RotationComponent >( Vector3f( 0.0f, 1.0f, 0.0f ), 0.25f * Numericf::HALF_PI );
    scene->attachNode( geometry );

    auto camera = crimild::alloc< Camera >();
    camera->local().setTranslate( Vector3f( 0.0f, 0.0f, 3.0f ) );
    scene->attachNode( camera );
    
    sim->setScene( scene );
	return sim->run();
}
*/


