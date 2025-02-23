using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

public class RayTracingTestRenderPass : MonoBehaviour
{
    // Start is called before the first frame update
    void Start()
    {
        RayTracingHelper.Init();
        StartCoroutine(RayTracingHelper.RayQueryRender());
    }
}
