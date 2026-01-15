using System.Collections.Generic;
using System;

using Toast;
using Source.Toast.Math;

namespace Sandbox
{
    public class PlanetScript : Entity
    {
        private Entity mCameraEntity;
        private TransformComponent mCameraTransform, mMarsTransform;
        private Quaternion mCameraOldRotation, mCameraOldTranslation, mMarsOldRotation, mMarsOldTranslation;

        void OnCreate()
        {
            //mCameraEntity = FindEntityByName("Camera");
            //mCameraTransform = mCameraEntity.GetComponent<TransformComponent>();
            //mMarsTransform = GetComponent<TransformComponent>();
            //mCameraOldRotation = mCameraTransform.Rotation;
            //mCameraOldTranslation = mCameraTransform.Translation;

            //mMarsOldRotation = mMarsTransform.Rotation;
            //mMarsOldTranslation = mMarsTransform.Translation;
        }

        void OnClick()
        {
        }

        void OnUpdate(float ts)
        {
            //if (mCameraTransform.Rotation != mCameraOldRotation || mCameraTransform.Translation != mCameraOldTranslation || mMarsTransform.Rotation != mMarsOldRotation || mMarsTransform.Translation != mMarsTransform.Translation)
            //{
            //}

            //mCameraOldRotation = mCameraTransform.Rotation; 
            //mCameraOldTranslation = mCameraTransform.Translation;

            //mMarsOldRotation = mMarsTransform.Rotation;
            //mMarsOldTranslation = mMarsTransform.Translation;
        }
    }
}

