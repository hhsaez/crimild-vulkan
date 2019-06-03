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
#include <array>

const int MAX_FRAMES_IN_FLIGHT = 2;

namespace crimild {

	namespace vulkan {

		struct Vertex {
			Vector2f pos;
			Vector3f color;

			static VkVertexInputBindingDescription getBindingDescription( void )
			{
				return VkVertexInputBindingDescription {
					.binding = 0,
					.stride = sizeof( Vertex ),
					.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
				};
			}

			static std::array< VkVertexInputAttributeDescription, 2 > getAttributeDescriptions( void )
			{
				return std::array< VkVertexInputAttributeDescription, 2 > {
					VkVertexInputAttributeDescription {
						.binding = 0,
						.location = 0,
						.format = VK_FORMAT_R32G32_SFLOAT,
						.offset = offsetof( Vertex, pos ),
					},
					VkVertexInputAttributeDescription {
						.binding = 0,
						.location = 1,
						.format = VK_FORMAT_R32G32B32_SFLOAT,
						.offset = offsetof( Vertex, color )
					},
				};
			}
		};

		struct UniformBufferObject {
			Matrix4f model;
			Matrix4f view;
			Matrix4f proj;
		};

		/**
		   \todo Move vkEnumerate* code to templates? Maybe using a lambda for the actual function?
		 */
		class VulkanSimulation {
		public:
			void run( void )
			{
				m_vertices = {
					{ { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
					{ { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
					{ { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
					{ { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f } },
				};

				m_indices = {
					0, 1, 2,
					2, 3, 0,
				};

				if ( !initWindow() ) {
					return;
				}
				
				initVulkan();
				loop();
				cleanup();
			}

			/**
			   \name Window setup
			 */
			//@{

		private:
            static void errorCallback( int error, const char* description )
            {
                std::cerr << "GLFW Error: (" << error << ") " << description << std::endl;
            }

			static void framebufferResizeCallback( GLFWwindow *window, int width, int height )
			{
				auto app = reinterpret_cast< VulkanSimulation * >( glfwGetWindowUserPointer( window ) );
				app->m_framebufferResized = true;
			}

			crimild::Bool initWindow( void)
			{
				glfwInit();
                glfwSetErrorCallback( errorCallback );
				glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
				glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

				_window = glfwCreateWindow( _width, _height, "Hello Vulkan!", nullptr, nullptr );

				glfwSetWindowUserPointer( _window, this );
				glfwSetFramebufferSizeCallback( _window, framebufferResizeCallback );

				return true;
			}

			//@}

		private:
			
			void initVulkan( void )
			{
				createInstance();
				setupDebugMessenger();
				createSurface();
				pickPhysicalDevice();
				createLogicalDevice();
				createSwapChain();
				createImageViews();
				createRenderPass();
				createDescriptorSetLayout();
				createGraphicsPipeline();
				createFramebuffers();
				createCommandPool();
				createVertexBuffer();
				createIndexBuffer();
				createUniformBuffers();
				createDescriptorPool();
				createDescriptorSets();
				createCommandBuffers();
				createSyncObjects();
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
						m_physicalDevice = device;
						break;
					}
				}

				if ( m_physicalDevice == VK_NULL_HANDLE ) {
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
			VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

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
					drawFrame();
				}

				vkDeviceWaitIdle( m_device );
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
				QueueFamilyIndices indices = findQueueFamilies( m_physicalDevice );
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

				if ( vkCreateDevice( m_physicalDevice, &createInfo, nullptr, &m_device ) != VK_SUCCESS ) {
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

				int width, height;
				glfwGetFramebufferSize( _window, &width, &height );

				VkExtent2D actualExtent = {
					static_cast< uint32_t >( width ),
					static_cast< uint32_t >( height ),
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
				
				auto swapChainSupport = querySwapChainSupport( m_physicalDevice );
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

				auto indices = findQueueFamilies( m_physicalDevice );
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

			void cleanupSwapChain( void )
			{
				for ( auto i = 0l; i < m_swapChainFramebuffers.size(); i++ ) {
					vkDestroyFramebuffer( m_device, m_swapChainFramebuffers[ i ], nullptr );
				}

				vkFreeCommandBuffers(
					m_device,
					m_commandPool,
					static_cast< uint32_t >( m_commandBuffers.size() ),
					m_commandBuffers.data()
				);

				vkDestroyPipeline( m_device, m_graphicsPipeline, nullptr );
				vkDestroyPipelineLayout( m_device, m_pipelineLayout, nullptr );
				vkDestroyRenderPass( m_device, m_renderPass, nullptr );

				for ( auto i = 0l; i < m_swapChainImageViews.size(); ++i ) {
					vkDestroyImageView( m_device, m_swapChainImageViews[ i ], nullptr );
				}

				vkDestroySwapchainKHR( m_device, m_swapChain, nullptr );

				for ( auto i = 0l; i < m_swapChainImages.size(); ++i ) {
					vkDestroyBuffer( m_device, m_uniformBuffers[ i ], nullptr );
					vkFreeMemory( m_device, m_uniformBuffersMemory[ i ], nullptr );
				}

				vkDestroyDescriptorPool( m_device, m_descriptorPool, nullptr );
			}

			void recreateSwapChain( void )
			{
				int width = 0;
				int height = 0;
				while ( width == 0 || height == 0 ) {
					glfwGetFramebufferSize( _window, &width, &height );
					glfwWaitEvents();
				}
				
				vkDeviceWaitIdle( m_device );

				createSwapChain();
				createImageViews();
				createRenderPass();
				createGraphicsPipeline();
				createFramebuffers();
				createUniformBuffers();
				createDescriptorPool();
				createDescriptorSets();
				createCommandBuffers();
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

		private:
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
			   \name Descriptor sets
			 */
			//@{
			
		private:

			void createDescriptorSetLayout( void )
			{
				auto uboLayoutBinding = VkDescriptorSetLayoutBinding {
					.binding = 0,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.pImmutableSamplers = nullptr,
				};

				auto layoutInfo = VkDescriptorSetLayoutCreateInfo {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
					.bindingCount = 1,
					.pBindings = &uboLayoutBinding,
				};

				if ( vkCreateDescriptorSetLayout( m_device, &layoutInfo, nullptr, &m_descriptorSetLayout ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create descriptor set layout" );
				}
			}

			void createDescriptorPool( void )
			{
				auto poolSize = VkDescriptorPoolSize {
					.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount = static_cast< uint32_t >( m_swapChainImages.size() ),
				};

				auto poolInfo = VkDescriptorPoolCreateInfo {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
					.poolSizeCount = 1,
					.pPoolSizes = &poolSize,
					.maxSets = static_cast< uint32_t >( m_swapChainImages.size() ),
				};

				if ( vkCreateDescriptorPool( m_device, &poolInfo, nullptr, &m_descriptorPool ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create descriptor pool" );
				}
			}

			void createDescriptorSets( void )
			{
				std::vector< VkDescriptorSetLayout > layouts( m_swapChainImages.size(), m_descriptorSetLayout );

				auto allocInfo = VkDescriptorSetAllocateInfo {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
					.descriptorPool = m_descriptorPool,
					.descriptorSetCount = static_cast< uint32_t >( m_swapChainImages.size() ),
					.pSetLayouts = layouts.data(),
				};

				m_descriptorSets.resize( m_swapChainImages.size() );

				if ( vkAllocateDescriptorSets( m_device, &allocInfo, m_descriptorSets.data() ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to allocate descriptor sets" );
				}

				for ( auto i = 0l; i < m_swapChainImages.size(); ++i ) {
					auto bufferInfo = VkDescriptorBufferInfo {
						.buffer = m_uniformBuffers[ i ],
						.offset = 0,
						.range = sizeof( UniformBufferObject ),
					};

					auto descriptorWrite = VkWriteDescriptorSet {
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = m_descriptorSets[ i ],
						.dstBinding = 0,
						.dstArrayElement = 0,
						.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
						.descriptorCount = 1,
						.pBufferInfo = &bufferInfo,
						.pImageInfo = nullptr,
						.pTexelBufferView = nullptr,
					};

					vkUpdateDescriptorSets( m_device, 1, &descriptorWrite, 0, nullptr );
				}
			}

		private:
			VkDescriptorSetLayout m_descriptorSetLayout;
			VkDescriptorPool m_descriptorPool;
			std::vector< VkDescriptorSet > m_descriptorSets;

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

				auto bindingDescription = Vertex::getBindingDescription();
				auto attributeDescriptions = Vertex::getAttributeDescriptions();

				auto vertexInputInfo = VkPipelineVertexInputStateCreateInfo {
					.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
					.vertexBindingDescriptionCount = 1,
					.pVertexBindingDescriptions = &bindingDescription,
					.vertexAttributeDescriptionCount = static_cast< uint32_t >( attributeDescriptions.size() ),
					.pVertexAttributeDescriptions = attributeDescriptions.data(),
				};

				// Input Assembly

				auto inputAssembly = VkPipelineInputAssemblyStateCreateInfo {
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
					.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
					.setLayoutCount = 1,
					.pSetLayouts = &m_descriptorSetLayout,
					.pushConstantRangeCount = 0,
					.pPushConstantRanges = nullptr,
				};

				if ( vkCreatePipelineLayout( m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create pipeline layout" );
				}

				auto pipelineInfo = VkGraphicsPipelineCreateInfo {
					.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
					.stageCount = 2,
					.pStages = shaderStages,
					.pVertexInputState = &vertexInputInfo,
					.pInputAssemblyState = &inputAssembly,
					.pViewportState = &viewportState,
					.pRasterizationState = &rasterizer,
					.pMultisampleState = &multisampling,
					.pDepthStencilState = nullptr,
					.pColorBlendState = &colorBlending,
					.pDynamicState = nullptr,
					.layout = m_pipelineLayout,
					.renderPass = m_renderPass,
					.subpass = 0,
					.basePipelineHandle = VK_NULL_HANDLE,
					.basePipelineIndex = -1,
				};

				if ( vkCreateGraphicsPipelines( m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create graphics pipeline" );
				}

				// Cleanup
				vkDestroyShaderModule( m_device, fragShaderModule, nullptr );
				vkDestroyShaderModule( m_device, vertShaderModule, nullptr );
			}

		private:
			VkPipelineLayout m_pipelineLayout;
			VkPipeline m_graphicsPipeline;

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

				// Subpass dependencies

				auto dependency = VkSubpassDependency {
					.srcSubpass = VK_SUBPASS_EXTERNAL,
					.dstSubpass = 0,
					.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.srcAccessMask = 0,
					.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				};
				
				// Render Pass

				auto renderPassInfo = VkRenderPassCreateInfo {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
					.attachmentCount = 1,
					.pAttachments = &colorAttachment,
					.subpassCount = 1,
					.pSubpasses = &subpass,
					.dependencyCount = 1,
					.pDependencies = &dependency,
				};

				if ( vkCreateRenderPass( m_device, &renderPassInfo, nullptr, &m_renderPass ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create render pass" );
				}

			}

		private:
			VkRenderPass m_renderPass;

			//@}
			
			/**
			   \name Framebuffers
			*/
			//@{

		private:
			void createFramebuffers( void )
			{
				m_swapChainFramebuffers.resize( m_swapChainImageViews.size() );

				for ( auto i = 0l; i < m_swapChainImageViews.size(); ++i ) {
					VkImageView attachments[] = {
						m_swapChainImageViews[ i ],
					};
					
					auto framebufferInfo = VkFramebufferCreateInfo {
						.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
						.renderPass = m_renderPass,
						.attachmentCount = 1,
						.pAttachments = attachments,
						.width = m_swapChainExtent.width,
						.height = m_swapChainExtent.height,
						.layers = 1,
					};
					
					if ( vkCreateFramebuffer( m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[ i ] ) != VK_SUCCESS ) {
						throw RuntimeException( "Failed to create framebuffer" );
					}
				}
			}

		private:
			std::vector< VkFramebuffer > m_swapChainFramebuffers;

			//@}

			/**
			   \name Command Pools
			 */
			//@{

		private:
			void createCommandPool( void )
			{
				auto queueFamilyIndices = findQueueFamilies( m_physicalDevice );

				auto poolInfo = VkCommandPoolCreateInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
					.queueFamilyIndex = queueFamilyIndices.graphicsFamily[ 0 ],
					.flags = 0,
				};

				if ( vkCreateCommandPool( m_device, &poolInfo, nullptr, &m_commandPool ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create command pool" );
				}
			}

		private:
			VkCommandPool m_commandPool;

			//@}

			/**
			   \name Buffers
			*/
			//@{

		private:
			void createBuffer( VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory )
			{
				auto bufferInfo = VkBufferCreateInfo {
					.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
					.size = size,
					.usage = usage,
					.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				};

				if ( vkCreateBuffer( m_device, &bufferInfo, nullptr, &buffer ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to create buffer" );
				}

				VkMemoryRequirements memRequirements;
				vkGetBufferMemoryRequirements( m_device, buffer, &memRequirements );

				auto allocInfo = VkMemoryAllocateInfo {
					.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
					.allocationSize = memRequirements.size,
					.memoryTypeIndex = findMemoryType( memRequirements.memoryTypeBits, properties ),
				};

				// TODO: we're not supposed to use vkAllocateMemory multiple times.
				// Instead, use a memory allocator and vkAllocateMemeory as few times as possible
				if ( vkAllocateMemory( m_device, &allocInfo, nullptr, &bufferMemory ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to allocate buffer memory" );					
				}

				vkBindBufferMemory( m_device, buffer, bufferMemory, 0 );
			}

			uint32_t findMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties )
			{
				VkPhysicalDeviceMemoryProperties memProperties;
				vkGetPhysicalDeviceMemoryProperties( m_physicalDevice, &memProperties );

				for ( uint32_t i = 0; i < memProperties.memoryTypeCount; ++i ) {
					if ( typeFilter & ( 1 << i ) && ( memProperties.memoryTypes[ i ].propertyFlags & properties ) == properties ) {
						return i;
					}
				}

				throw RuntimeException( "Failed to find suitable memory type" );
			}

			void copyBuffer( VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size )
			{
				// TODO: might be better to create a different pool for coping buffers
				
				auto allocInfo = VkCommandBufferAllocateInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandPool = m_commandPool,
					.commandBufferCount = 1,
				};

				VkCommandBuffer commandBuffer;
				vkAllocateCommandBuffers( m_device, &allocInfo, &commandBuffer );

				auto beginInfo = VkCommandBufferBeginInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				};

				vkBeginCommandBuffer( commandBuffer, &beginInfo );

				auto copyRegion = VkBufferCopy {
					.srcOffset = 0,
					.dstOffset = 0,
					.size = size,
				};

				vkCmdCopyBuffer( commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion );

				vkEndCommandBuffer( commandBuffer );

				auto submitInfo = VkSubmitInfo {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 1,
					.pCommandBuffers = &commandBuffer,
				};

				vkQueueSubmit( m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
				vkQueueWaitIdle( m_graphicsQueue );

				vkFreeCommandBuffers( m_device, m_commandPool, 1, &commandBuffer );
			}

			void createVertexBuffer( void )
			{
				VkDeviceSize bufferSize = sizeof( m_vertices[ 0 ] ) * m_vertices.size();

				VkBuffer stagingBuffer;
				VkDeviceMemory stagingBufferMemory;
				
				createBuffer(
					bufferSize,
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					stagingBuffer,
					stagingBufferMemory
				);

				void *data;
				vkMapMemory( m_device, stagingBufferMemory, 0, bufferSize, 0, &data );
				memcpy( data, m_vertices.data(), ( size_t ) bufferSize );
				vkUnmapMemory( m_device, stagingBufferMemory );
				
				createBuffer(
					bufferSize,
					VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					m_vertexBuffer,
					m_vertexBufferMemory
				);

				copyBuffer( stagingBuffer, m_vertexBuffer, bufferSize );

				vkDestroyBuffer( m_device, stagingBuffer, nullptr );
				vkFreeMemory( m_device, stagingBufferMemory, nullptr );
			}

			void createIndexBuffer()
			{
				VkDeviceSize bufferSize = sizeof( m_indices[ 0 ] ) * m_indices.size();

				VkBuffer stagingBuffer;
				VkDeviceMemory stagingBufferMemory;
				createBuffer(
					bufferSize,
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					stagingBuffer,
					stagingBufferMemory
				);

				void *data;
				vkMapMemory( m_device, stagingBufferMemory, 0, bufferSize, 0, &data );
				memcpy( data, m_indices.data(), ( size_t ) bufferSize );
				vkUnmapMemory( m_device, stagingBufferMemory );

				createBuffer(
					bufferSize,
					VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					m_indexBuffer,
					m_indexBufferMemory
				);

				copyBuffer( stagingBuffer, m_indexBuffer, bufferSize );

				vkDestroyBuffer( m_device, stagingBuffer, nullptr );
				vkFreeMemory( m_device, stagingBufferMemory, nullptr );
			}

			void createUniformBuffers( void )
			{
				VkDeviceSize bufferSize = sizeof( UniformBufferObject );

				m_uniformBuffers.resize( m_swapChainImages.size() );
				m_uniformBuffersMemory.resize( m_swapChainImages.size() );

				for ( auto i = 0l; i < m_swapChainImages.size(); ++i ) {
					createBuffer(
						bufferSize,
						VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						m_uniformBuffers[ i ],
						m_uniformBuffersMemory[ i ]
					);
				}
			}

			void updateUniformBuffer( uint32_t currentImage )
			{
				static auto startTime = std::chrono::high_resolution_clock::now();

				auto currentTime = std::chrono::high_resolution_clock::now();
				auto time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();

				auto ubo = UniformBufferObject { };

				// Model
				ubo.model = []( crimild::Real32 time ) {
					Transformation t;
					t.rotate().fromAxisAngle( Vector3f::UNIT_Z, time * 35.0f * Numericf::DEG_TO_RAD );
					return t.computeModelMatrix();
				}( time );

				auto lookAt = []( const Vector3f &eye, const Vector3f &target, const Vector3f &up ) -> Matrix4f {
					auto z = ( target - eye ).getNormalized();
					auto y = up.getNormalized();
					auto x = ( z ^ y ).getNormalized();
					y = ( x ^ z ).getNormalized();
					z *= -1;

					return Matrix4f(
						x.x(), y.x(), z.x(), 0.0f,
						x.y(), y.y(), z.y(), 0.0f,
						x.z(), y.z(), z.z(), 0.0f,
						-( x * eye ), -( y * eye ), -( z * eye ), 1.0f
					);
				};

				// View
				ubo.view = lookAt(
					Vector3f( 2.0f, 2.0f, 2.0f ),
					Vector3f( 0.0f, 0.0f, 0.0f ),
					Vector3f( 0.0f, 1.0f, 0.0f )
				);

				// Projection
				ubo.proj = []( float width, float height ) {
					auto frustum = Frustumf( 45.0f, width / height, 0.1f, 100.0f );
					auto proj = frustum.computeProjectionMatrix();

					// Invert Y-axis
					// This also needs to set front face as counter-clockwise for culling
					// when configuring pipeline
					// Also: https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
					proj[ 5 ] *= -1.0f;

					return proj;
				}( m_swapChainExtent.width, m_swapChainExtent.height );
				
				void *data;
				vkMapMemory( m_device, m_uniformBuffersMemory[ currentImage ], 0, sizeof( ubo ), 0, &data );
				memcpy( data, &ubo, sizeof( ubo ) );
				vkUnmapMemory( m_device, m_uniformBuffersMemory[ currentImage ] );
			}

		private:
			std::vector< Vertex > m_vertices;
			std::vector< uint16_t > m_indices;

			VkBuffer m_vertexBuffer;
			VkDeviceMemory m_vertexBufferMemory;
			VkBuffer m_indexBuffer;
			VkDeviceMemory m_indexBufferMemory;

			std::vector< VkBuffer > m_uniformBuffers;
			std::vector< VkDeviceMemory > m_uniformBuffersMemory;

			//@}

			/**
			   \name Command Buffers
			*/
			//@{

		private:
			void createCommandBuffers( void )
			{
				m_commandBuffers.resize( m_swapChainFramebuffers.size() );

				auto allocInfo = VkCommandBufferAllocateInfo {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
					.commandPool = m_commandPool,
					.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					.commandBufferCount = ( uint32_t ) m_commandBuffers.size(),
				};

				if ( vkAllocateCommandBuffers( m_device, &allocInfo, m_commandBuffers.data() ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to allocate command buffers" );
				}

				for ( auto i = 0l; i < m_commandBuffers.size(); ++i ) {
					auto beginInfo = VkCommandBufferBeginInfo {
						.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
						.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
						.pInheritanceInfo = nullptr,
					};

					if ( vkBeginCommandBuffer( m_commandBuffers[ i ], &beginInfo ) != VK_SUCCESS ) {
						throw RuntimeException( "Failed to begin recording command buffer" );
					}

					auto clearColor = VkClearValue { 0.0f, 0.0f, 0.0f, 1.0f };

					auto renderPassInfo = VkRenderPassBeginInfo {
						.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
						.renderPass = m_renderPass,
						.framebuffer = m_swapChainFramebuffers[ i ],
						.renderArea.offset = { 0, 0 },
						.renderArea.extent = m_swapChainExtent,
						.clearValueCount = 1,
						.pClearValues = &clearColor,
					};

					vkCmdBeginRenderPass( m_commandBuffers[ i ], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

					vkCmdBindPipeline( m_commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline );

					// bind vertex buffers
					VkBuffer vertexBuffers[] = { m_vertexBuffer };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers( m_commandBuffers[ i ], 0, 1, vertexBuffers, offsets );

					// bind index buffer
					vkCmdBindIndexBuffer( m_commandBuffers[ i ], m_indexBuffer, 0, VK_INDEX_TYPE_UINT16 );

					// bind uniform buffers
					vkCmdBindDescriptorSets(
						m_commandBuffers[ i ],
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						m_pipelineLayout,
						0,
						1,
						&m_descriptorSets[ i ],
						0,
						nullptr
					);

					vkCmdDrawIndexed(
						m_commandBuffers[ i ],
						static_cast< uint32_t >( m_indices.size() ),
						1,
						0,
						0,
						0
					);

					vkCmdEndRenderPass( m_commandBuffers[ i ] );

					if ( vkEndCommandBuffer( m_commandBuffers[ i ] ) != VK_SUCCESS ) {
						throw RuntimeException( "Failed to record command buffer" );
					}
				}
			}

		private:
			/**
			   Command buffers are automatically freed when their command pool
			   is destroyed, so there's no need for an explicit cleanup.
			 */
			std::vector< VkCommandBuffer > m_commandBuffers;

			//@}

			/**
			   \name Semaphores and Fences
			 */
			//@{

		private:
			void createSyncObjects( void )
			{
				m_imageAvailableSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
				m_renderFinishedSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
				m_inFlightFences.resize( MAX_FRAMES_IN_FLIGHT );
				
				auto semaphoreInfo = VkSemaphoreCreateInfo {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				};

				auto fenceInfo = VkFenceCreateInfo {
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
					.flags = VK_FENCE_CREATE_SIGNALED_BIT,
				};

				for ( auto i = 0l; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
					if ( vkCreateSemaphore( m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[ i ] ) != VK_SUCCESS ||
						vkCreateSemaphore( m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[ i ] ) != VK_SUCCESS ||
						vkCreateFence( m_device, &fenceInfo, nullptr, &m_inFlightFences[ i ] ) != VK_SUCCESS ) {
						throw RuntimeException( "Failed to create synchronization objects" );
					}
				}
			}

		private:
			std::vector< VkSemaphore > m_imageAvailableSemaphores;
			std::vector< VkSemaphore > m_renderFinishedSemaphores;
			std::vector< VkFence > m_inFlightFences;

			//@}

			/**
			  \name Render frame
			*/
			//@{

		private:
			void drawFrame( void )
			{
				// Wait for previous frame to be finished
				vkWaitForFences(
					m_device,
					1,
					&m_inFlightFences[ m_currentFrame ],
					VK_TRUE,
					std::numeric_limits< uint64_t >::max()
				);

				// Acquire image from swap chain
				uint32_t imageIndex;
				auto result = vkAcquireNextImageKHR(
					m_device,
					m_swapChain,
					std::numeric_limits< uint64_t >::max(),
					m_imageAvailableSemaphores[ m_currentFrame ],
					VK_NULL_HANDLE,
					&imageIndex
				);

				if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
					// The swap chain has become incompatible with the surface
					// and can no longer be used for rendering (window size changed?)
					recreateSwapChain();
					return;
				}
				else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
					throw RuntimeException( "Failed to acquire swap chain image" );
				};

				// Updating uniform buffers
				updateUniformBuffer( imageIndex );

				// Submitting the command buffer

				VkSemaphore waitSemaphores[] = {
					m_imageAvailableSemaphores[ m_currentFrame ],
				};

				VkPipelineStageFlags waitStages[] = {
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				};
				
				VkSemaphore signalSemaphores[] = {
					m_renderFinishedSemaphores[ m_currentFrame ],
				};
				
				auto submitInfo = VkSubmitInfo {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = waitSemaphores,
					.pWaitDstStageMask = waitStages,
					.commandBufferCount = 1,
					.pCommandBuffers = &m_commandBuffers[ imageIndex ],
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = signalSemaphores,
				};

				vkResetFences( m_device, 1, &m_inFlightFences[ m_currentFrame ] );
				
				if ( vkQueueSubmit( m_graphicsQueue, 1, &submitInfo, m_inFlightFences[ m_currentFrame ] ) != VK_SUCCESS ) {
					throw RuntimeException( "Failed to submit draw command buffer" );
				}

				// Presentation

				VkSwapchainKHR swapChains[] = { m_swapChain };

				auto presentInfo = VkPresentInfoKHR {
					.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = signalSemaphores,
					.swapchainCount = 1,
					.pSwapchains = swapChains,
					.pImageIndices = &imageIndex,
					.pResults = nullptr,
				};

				result = vkQueuePresentKHR( m_presentQueue, &presentInfo );
				if ( result != VK_SUCCESS ) {
					if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized ) {
						// We want the best possible result, so recreate swapchain in both cases
						recreateSwapChain();
						m_framebufferResized = false;
					}
					else {
						throw RuntimeException( "Failed to present swap chain image" );
					}
				}

				m_currentFrame = ( m_currentFrame + 1 ) % MAX_FRAMES_IN_FLIGHT;
			}

		private:
			size_t m_currentFrame = 0;
			bool m_framebufferResized = false;

			//@}

			/**
			   \name Cleanup
			*/
			//@{
			
		private:
			void cleanup( void )
			{
				cleanupSwapChain();

				vkDestroyDescriptorSetLayout( m_device, m_descriptorSetLayout, nullptr );

				vkDestroyBuffer( m_device, m_vertexBuffer, nullptr );
				vkFreeMemory( m_device, m_vertexBufferMemory, nullptr );
				vkDestroyBuffer( m_device, m_indexBuffer, nullptr );
				vkFreeMemory( m_device, m_indexBufferMemory, nullptr );
				
				for ( auto i = 0l; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
					vkDestroySemaphore( m_device, m_renderFinishedSemaphores[ i ], nullptr );
					vkDestroySemaphore( m_device, m_imageAvailableSemaphores[ i ], nullptr );
					vkDestroyFence( m_device, m_inFlightFences[ i ], nullptr );
				}

				vkDestroyCommandPool( m_device, m_commandPool, nullptr );
				
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


