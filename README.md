# Unity_NativePlugin_Vulkan_Mobile_RayQuery
Unity NativePlugin 继承Vulkan 原生移动端支持的RayQuery 光线追踪实现
基于官方Vulkan 范例RayQuery


![Alt text](https://github.com/dreamfairy/Unity_NativePlugin_Vulkan_Mobile_RayQuery/blob/main/Preview/s.png?raw=true)
![Alt text](https://github.com/dreamfairy/Unity_NativePlugin_Vulkan_Mobile_RayQuery/blob/main/Preview/preview.gif?raw=true)

完全实现了AO和阴影
[官方范例地址](https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/extensions/ray_queries)


使用的Unity Native Log库
[UnityNativeLogger]https://github.com/Darkgrouptw/UnityNativeLogger

参考项目（仅支持PC)
[基于RayPipeline光追]https://github.com/afuzzyllama/UnityRayTracingPlugin

没有文档的辣鸡Unity渲染范例
[UnityNativeRenderingPlugin]https://github.com/Unity-Technologies/NativeRenderingPlugin


<h3>使用方式</h3>

1.打开Unity工程

2.打开RayQuery场景

3.运行

4.可以实时控制灯光，和场景物件。

<h3>不支持，也不考虑支持，这个项目是为了参考接入RayQuery,因此不会实现多余的功能，增加其他同学参考复杂度</h3>

1.目前只支持1个直线光

2.目前只支持1个相机


最近Unity6000 官方又开始发病了，很多功能不考虑等官方了。还是自己接更稳妥些。

话说距离手机硬件芯片支持硬件光追已经过去2年了，目前支持Vulkan1.2 的芯片至少从 8gen2 起步，差不多是时候拥抱硬件光追了。

顺便在说下硬件光追目前实际上有2个扩展，而目前的手机厂商基本都只支持VK_KHR_ray_query，切记不要写错

一个是VK_KHR_ray_query

一个是VK_KHR_ray_tracing_pipeline

VK_KHR_ray_query的光追依然和传统渲染管线一样，在每个几何体自己的Vert/Frag Shader 自己发射射线并通过调用

rayQueryInitializeEXT/rayQueryGetIntersectionTypeEXT 等一系列API查询光追结果

VK_KHR_ray_tracing_pipeline 则完全不一样

它需要一次性准备好 射线命中/射线生成/射线错过/阴影命中/阴影错过 的Shader,然后通过vkCmdTraceRaysKHR直接发起整个光追管线

渲染， 和传统管线完全不一样，有点像延迟渲染的感觉

目前全网没有找到移动端光追 RayQuery的Unity接入文档，也就是本文发布的起因之一

