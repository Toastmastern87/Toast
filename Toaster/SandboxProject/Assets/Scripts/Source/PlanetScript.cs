using System.Collections.Generic;
using System;

using Toast;

namespace Sandbox
{
    public class PlanetScript : Entity
    {
        private Entity mCameraEntity;
        private TransformComponent mCameraTransform;
        private Vector3 mCameraOldRotation;

        private PlanetComponent mPlanetComponent;

        void OnCreate()
        {
            mCameraEntity = FindEntityByName("Camera");
            mCameraTransform = mCameraEntity.GetComponent<TransformComponent>();
            mCameraOldRotation = mCameraTransform.Rotation;

            mPlanetComponent = GetComponent<PlanetComponent>();
        }

        void OnClick()
        {
        }

        void OnUpdate(float ts)
        {
            if (mCameraTransform.Rotation != mCameraOldRotation)
            {


                //mPlanetComponent.RegeneratePlanet(mCameraTransform.Translation, mCameraTransform.Transform);
            }

            mCameraOldRotation = mCameraTransform.Rotation;
        }
    }
}
