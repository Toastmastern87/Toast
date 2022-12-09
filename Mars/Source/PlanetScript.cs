using System.Collections.Generic;
using System;

using Toast;

class PlanetScript : EntityOld
{
    private EntityOld mCameraEntity;
    private TransformComponent mCameraTransform;
    private Matrix4 mCameraOldTransform;

    private PlanetComponent mPlanetComponent;

    void OnCreate() 
    {
        mCameraEntity = FindEntityByTag("Camera");
        mCameraTransform = mCameraEntity.GetComponent<TransformComponent>();
        mCameraOldTransform = mCameraTransform.Transform;

        mPlanetComponent = GetComponent<PlanetComponent>();
    }

    void OnClick()
    {
    }

    void OnUpdate(float ts)
    {
        if (mCameraTransform.Transform != mCameraOldTransform) 
        {
            Vector3 cameraPos = new Vector3(mCameraTransform.Transform.D03, mCameraTransform.Transform.D13, mCameraTransform.Transform.D23);
            Vector3 cameraForward = new Vector3(mCameraTransform.Transform.D03, mCameraTransform.Transform.D13, mCameraTransform.Transform.D23);
            mPlanetComponent.RegeneratePlanet(cameraPos, mCameraTransform.Transform);
        }

        mCameraOldTransform = mCameraTransform.Transform;
    }
}

