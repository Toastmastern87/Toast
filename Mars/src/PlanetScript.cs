using System.Collections.Generic;
using System;

using Toast;

class PlanetScript : Entity
{
    private Entity mCameraEntity;
    private TransformComponent mCameraTransform;
    private Matrix4 mCameraOldTransform;

    private MeshComponent mMeshComponent;

    void OnCreate() 
    {
        mCameraEntity = FindEntityByTag("Camera");
        mCameraTransform = mCameraEntity.GetComponent<TransformComponent>();
        mCameraOldTransform = mCameraTransform.Transform;

        mMeshComponent = GetComponent<MeshComponent>();
    }

    void OnUpdate(float ts)
    {
        if (mCameraTransform.Transform != mCameraOldTransform) 
        {
            Vector3 cameraPos = new Vector3(mCameraTransform.Transform.D03, mCameraTransform.Transform.D13, mCameraTransform.Transform.D23);
            Vector3 cameraForward = new Vector3(mCameraTransform.Transform.D03, mCameraTransform.Transform.D13, mCameraTransform.Transform.D23);
           // Toast.Console.LogInfo("Camera forward vector: " + mCameraTransform.Transform.ToString());
            mMeshComponent.RegeneratePlanet(cameraPos, mCameraTransform.Transform);
        }

        mCameraOldTransform = mCameraTransform.Transform;
    }
}

