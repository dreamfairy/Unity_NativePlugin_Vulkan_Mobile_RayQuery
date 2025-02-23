using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Unity.Collections;
using UnityEngine;

[ExecuteInEditMode]
[RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
public class RayTracingMeshFilter : MonoBehaviour
{
    [ReadOnly]
    public int GameObjectId = -1;
    [ReadOnly]
    public int SharedMeshInstanceID = -1;
    
    [ReadOnly]

    public bool SharedMeshRegisteredWithRayTracer = false;
    
    private MeshFilter m_meshFilter;
    private bool m_hasCreateTLAS = false;
    

    void OnEnable()
    {
        Debug.Log("OnEnable RTMeshFilter");
        Init();
    }

    private void OnDisable()
    {
        RayTracingHelper.RemoveTlasInstance(this.GameObjectId);
    }

    void Init()
    {
        m_hasCreateTLAS = false;
        m_meshFilter = this.gameObject.GetComponent<UnityEngine.MeshFilter>();
        this.GameObjectId = this.gameObject.GetInstanceID();
        this.SharedMeshInstanceID = m_meshFilter.GetInstanceID();
        
        CreateRayTracingData();
    }

    void CreateRayTracingData()
    {
        m_meshFilter.sharedMesh.RecalculateNormals();
        m_meshFilter.sharedMesh.RecalculateTangents();
        m_meshFilter.sharedMesh.RecalculateBounds();
        
        var vertices = m_meshFilter.sharedMesh.vertices;
        var normals = m_meshFilter.sharedMesh.normals;
        var tangents = m_meshFilter.sharedMesh.tangents;
        var uv = m_meshFilter.sharedMesh.uv;
        var indices = m_meshFilter.sharedMesh.triangles;
        
        var verticesHandle = GCHandle.Alloc(vertices, GCHandleType.Pinned);
        var normalsHandle = GCHandle.Alloc(normals, GCHandleType.Pinned);
        var tangentsHandle = GCHandle.Alloc(tangents, GCHandleType.Pinned);
        var uvsHandle = GCHandle.Alloc(uv, GCHandleType.Pinned);
        var indicesHandle = GCHandle.Alloc(indices, GCHandleType.Pinned);
        
        SharedMeshRegisteredWithRayTracer = RayTracingHelper.AddMeshToRayTracingSystem(
            this.SharedMeshInstanceID,
            verticesHandle.AddrOfPinnedObject(),
            normalsHandle.AddrOfPinnedObject(),
            tangentsHandle.AddrOfPinnedObject(),
            uvsHandle.AddrOfPinnedObject(),
            indicesHandle.AddrOfPinnedObject(),
            vertices.Length,
            indices.Length
        );
        
        verticesHandle.Free();
        normalsHandle.Free();
        tangentsHandle.Free();
        uvsHandle.Free();
        indicesHandle.Free();
        
        CreateTLAS();
    }
    
    void CreateTLAS()
    {
        var local2world = this.transform.localToWorldMatrix;
        var world2local = this.transform.worldToLocalMatrix;
        
        var local2worldHandle = GCHandle.Alloc(local2world, GCHandleType.Pinned);
        var world2localHandle = GCHandle.Alloc(world2local, GCHandleType.Pinned);
        
        m_hasCreateTLAS = RayTracingHelper.CreateTlAS(
            this.GameObjectId,
            this.SharedMeshInstanceID,
            local2worldHandle.AddrOfPinnedObject(),
            world2localHandle.AddrOfPinnedObject()
        );
        
        Debug.LogFormat("GameObject {0} Create TLAS {1}",this.GameObjectId, m_hasCreateTLAS);
        
        local2worldHandle.Free();
        world2localHandle.Free();
    }

    void UpdateLocal2World()
    {
        var local2world = this.transform.localToWorldMatrix;
        var local2worldHandle = GCHandle.Alloc(local2world, GCHandleType.Pinned);
        RayTracingHelper.UpdateModelMat(this.SharedMeshInstanceID, local2worldHandle.AddrOfPinnedObject());
        local2worldHandle.Free();
    }

    void UpdateTLASTRS()
    {
        if (!m_hasCreateTLAS)
        {
            return;
        }
        
        var local2world = this.transform.localToWorldMatrix;
        var world2local = this.transform.worldToLocalMatrix;
        
        var local2worldHandle = GCHandle.Alloc(local2world, GCHandleType.Pinned);
        var world2localHandle = GCHandle.Alloc(world2local, GCHandleType.Pinned);
        
        RayTracingHelper.UpdateTRSAndTLASToRayTracingSystem(
            this.GameObjectId,
            local2worldHandle.AddrOfPinnedObject(),
            world2localHandle.AddrOfPinnedObject()
        );
        
        local2worldHandle.Free();
        world2localHandle.Free();
    }

    // Update is called once per frame
    void Update()
    {
        UpdateLocal2World();
        UpdateTLASTRS();
    }
}
