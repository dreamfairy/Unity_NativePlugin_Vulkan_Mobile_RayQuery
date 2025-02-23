using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

public class RayTracingHelper
{

   [DllImport("RenderingPlugin")]
   public static extern void SetShaderData(int type, IntPtr shaderData, int dataLength);

   [DllImport("RenderingPlugin")]
   public static extern void Prepare();

   [DllImport("RenderingPlugin")]
   public static extern int AddLight(
      int lightId,
      float x,
      float y,
      float z,
      float dx,
      float dy,
      float dz,
      float r,
      float g,
      float b,
      float bounceIntensity,
      float intensity,
      float range,
      float spotAngle,
      int type,
      bool enable);

   [DllImport("RenderingPlugin")]
   public static extern void RemoveLight(int lightInstanceId);

   [DllImport("RenderingPlugin")]
   public static extern void UpdateLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz,
      float r, float g, float b, float bounceIntensity,
      float intensity, float range, float spotAngle, int type, bool enabled);

   [DllImport("RenderingPlugin")]
   public static extern int AddSharedMesh(int sharedMeshInstanceId, IntPtr verticesArray, IntPtr normalsArray,
      IntPtr tangentsArray, IntPtr uvsArray, int vertexCount, IntPtr indicesArray, int indexCount);

   [DllImport("RenderingPlugin")]
   public static extern int AddTlasInstance(int gameObjectInstanceId, int sharedMeshInstanceId,
      IntPtr l2wMatrix, IntPtr w2lMatrix);

   [DllImport("RenderingPlugin")]
   public static extern void RemoveTlasInstance(int gameObjectInstanceId);

   [DllImport("RenderingPlugin")]
   public static extern void UpdateTlasInstance(int gameObjectInstanceId, IntPtr l2wMatrix,
      IntPtr w2lMatrix);

   [DllImport("RenderingPlugin")]
   public static extern void UpdateCameraMat(float x, float y, float z, IntPtr w2camProj);

   [DllImport("RenderingPlugin")]
   public static extern void UpdateModelMat(int sharedMeshInstanceId, IntPtr l2w);

   [DllImport("RenderingPlugin")]
   public static extern IntPtr GetEventAndDataFunc();

   [DllImport("RenderingPlugin")]
   public static extern void SetLogger(IntPtr fp);

   [DllImport("RenderingPlugin")]
   public static extern void SetIntLogger(IntPtr fp);

   [DllImport("RenderingPlugin")]
   public static extern void TestLogger();


   [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
   private delegate void LogDelegate(int level, string str);

   [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
   private delegate void LogDelegateInt(int level, int str);

   static LogDelegate callBack_delegate = new LogDelegate(LogCallBack);
   static LogDelegateInt callBackInt_delegate = new LogDelegateInt(LogCallBackInt);

   public static void InitLog()
   {
      if (Application.isPlaying)
      {
         IntPtr delegatePtr = Marshal.GetFunctionPointerForDelegate(callBack_delegate);
         SetLogger(delegatePtr);

         IntPtr delegateIntPtr = Marshal.GetFunctionPointerForDelegate(callBackInt_delegate);
         SetIntLogger(delegateIntPtr);

         TestLogger();
      }
   }

   public static void DestroyLog()
   {
      SetLogger(IntPtr.Zero);
      SetIntLogger(IntPtr.Zero);
   }

   static void LogCallBack(int level, string msg)
   {
      if (level == 0)
      {
         Debug.Log(msg);
      }
      else if (level == 1)
      {
         Debug.LogWarning(msg);
      }
      else if (level == 2)
      {
         Debug.LogError(msg);
      }
   }

   static void LogCallBackInt(int level, int msg)
   {
      if (level == 0)
      {
         Debug.Log(msg);
      }
      else if (level == 1)
      {
         Debug.LogWarning(msg);
      }
      else if (level == 2)
      {
         Debug.LogError(msg);
      }
   }

   public static bool AddMeshToRayTracingSystem(
      int meshId,
      IntPtr verticesPtr,
      IntPtr normalsPtr,
      IntPtr tangentsPtr,
      IntPtr uvsPtr,
      IntPtr indicesPtr,
      int totalVertices,
      int totalIndices
   )
   {
      int ret = AddSharedMesh(
         meshId,
         verticesPtr,
         normalsPtr,
         tangentsPtr,
         uvsPtr,
         totalVertices,
         indicesPtr,
         totalIndices
      );
      return ret > 0;
   }

   public static bool CreateTlAS(int gameobjectId, int meshId,
      IntPtr local2worldPtr,
      IntPtr world2localPtr)
   {
      int ret = AddTlasInstance(gameobjectId, meshId, local2worldPtr, world2localPtr);
      return ret > 0;
   }

   public static void RemoveTLAS(int gameobjectId)
   {
      RemoveTlasInstance(gameobjectId);
   }

   public static void UpdateTRSAndTLASToRayTracingSystem(
      int gameobjectId,
      IntPtr local2worldPtr,
      IntPtr world2localPtr
   )
   {
      UpdateTlasInstance(gameobjectId, local2worldPtr, world2localPtr);
   }
   
   private static string[] needShader =
      { "ray_shadowVert", "ray_shadowFrag"};
    
   public static void LoadShaderData()
   {
      string fullPath = System.IO.Path.GetFullPath("Assets/Resources/CompiledShader/");
      
      for (int i = 0; i < needShader.Length; i++)
      {
         var data = Resources.Load<TextAsset>("CompiledShader/" + needShader[i]);
         if (data)
         {
            Debug.LogFormat("Find Data {0} len {1}", needShader[i], data.bytes.Length);
            byte[] byteData = data.bytes;
            GCHandle byteDataHandle = GCHandle.Alloc(byteData, GCHandleType.Pinned);
            RayTracingHelper.SetShaderData(i, byteDataHandle.AddrOfPinnedObject(), byteData.Length);
            byteDataHandle.Free();
         }
      }
   }
   
   private static void CreateOrUpdateLight(bool isCreate)
   {
      GameObject obj = GameObject.Find("Directional Light");
      if (!obj) return;
      Light light = obj.GetComponent<Light>();
      int lightId = light.GetInstanceID();
      var lightPos = light.transform.position;
      var lightDir = light.transform.forward;
      var lightColor = light.color;

      if (isCreate)
      {
         AddLight(lightId, 
            lightPos.x, lightPos.y, lightPos.z, 
            lightDir.x, lightDir.y, lightDir.z,
            lightColor.r, lightColor.g, lightColor.b, light.bounceIntensity,
            light.intensity, light.range, light.spotAngle, (int)light.type, light.enabled
         );
      }
      else
      {
         UpdateLight(lightId, 
            lightPos.x, lightPos.y, lightPos.z, 
            lightDir.x, lightDir.y, lightDir.z,
            lightColor.r, lightColor.g, lightColor.b, light.bounceIntensity,
            light.intensity, light.range, light.spotAngle, (int)light.type, light.enabled
         );
      }
   }
   
   public static void Init()
   {
      CreateOrUpdateLight(true);
      LoadShaderData();
      Prepare();
   }

   public static IEnumerator RayQueryRender()
   {
      while (true)
      {
         // Wait until all frame rendering is done
         yield return new WaitForEndOfFrame();

         Camera m_camera = Camera.main;

         var camPos = m_camera.transform.position;
         var w2cam = m_camera.worldToCameraMatrix;

         //flip vulkan top down inverse
         var proj = GL.GetGPUProjectionMatrix(m_camera.projectionMatrix, true);

         var w2camProj = proj * w2cam;
         var w2camProjHandle = GCHandle.Alloc(w2camProj, GCHandleType.Pinned);
        
         RayTracingHelper.UpdateCameraMat(camPos.x, camPos.y, camPos.z, w2camProjHandle.AddrOfPinnedObject());
         w2camProjHandle.Free();    
         CreateOrUpdateLight(false);
         
         GL.IssuePluginEvent(GetEventAndDataFunc(), 1);
      }
   }
}
