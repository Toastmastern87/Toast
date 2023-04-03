using System.Collections.Generic;
using System;

using Toast;

namespace Sandbox
{
    public class PlanetScript : Entity
    {
        private Entity mCameraEntity;
        private TransformComponent mCameraTransform, mMarsTransform;
        private Vector3 mCameraOldRotation, mCameraOldTranslation, mMarsOldRotation, mMarsOldTranslation;

        private PlanetComponent mPlanetComponent;

        void OnCreate()
        {
            mCameraEntity = FindEntityByName("Camera");
            mCameraTransform = mCameraEntity.GetComponent<TransformComponent>();
            mMarsTransform = GetComponent<TransformComponent>();
            mCameraOldRotation = mCameraTransform.Rotation;
            mCameraOldTranslation = mCameraTransform.Translation;

            mMarsOldRotation = mMarsTransform.Rotation;
            mMarsOldTranslation = mMarsTransform.Translation;

            mPlanetComponent = GetComponent<PlanetComponent>();
        }

        void OnClick()
        {
        }

        void OnUpdate(float ts)
        {
            if (mCameraTransform.Rotation != mCameraOldRotation || mCameraTransform.Translation != mCameraOldTranslation || mMarsTransform.Rotation != mMarsOldRotation || mMarsTransform.Translation != mMarsTransform.Translation)
            {
                mPlanetComponent.RegeneratePlanet(mCameraTransform.Translation, mCameraTransform.GetTransform());
            }

            mCameraOldRotation = mCameraTransform.Rotation;
            mCameraOldTranslation = mCameraTransform.Translation;

            mMarsOldRotation = mMarsTransform.Rotation;
            mMarsOldTranslation = mMarsTransform.Translation;
        }
    }
}
