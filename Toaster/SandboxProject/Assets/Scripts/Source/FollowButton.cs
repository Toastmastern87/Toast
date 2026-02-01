using System;
using System.Runtime.InteropServices;

using Toast;

namespace Sandbox
{
    public class FollowButton : Entity
    {
        private Entity mCamera;
        private Entity mInfoPanel;
        private Entity mStarship;
        private IntPtr mCameraScriptHandle;
        private CameraController mCameraInstance;
        private bool mCameraResolved = false;

        private TransformComponent mStarshipTransformComponent;

        void OnCreate()
        {
            mInfoPanel = this.FindParentEntity(this.ID);
            mStarship = mInfoPanel.FindParentEntity(mInfoPanel.ID);

            mStarshipTransformComponent = mStarship.GetComponent<TransformComponent>();
        }

        void OnEvent()
        {
            if (mCameraResolved && mCameraInstance != null)
                mCameraInstance.StartFollowing(mStarshipTransformComponent);
        }

        void OnUpdate(float ts)
        {
            if (!mCameraResolved)
            {
                mCamera = FindEntityByName("Camera");
                mCameraScriptHandle = mCamera.GetComponent<ScriptComponent>().ScriptInstance;

                GCHandle gch = GCHandle.FromIntPtr(mCameraScriptHandle);

                mCameraInstance = gch.Target as CameraController;
                if (mCameraInstance == null)
                    throw new Exception("Retrieved script instance is not of type Camera");

                mCameraResolved = true;
            }
        }
    }
}