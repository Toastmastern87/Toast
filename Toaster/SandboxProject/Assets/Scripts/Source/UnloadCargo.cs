using Sandbox.Source;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace Sandbox
{
    public class UnloadCargo : Entity
    {
        private Entity mInfoPanel;
        private Entity mStarship;
        private IntPtr mStarshipScriptHandle;
        private StarshipLanding mStarshipLandingInstance;
        private MeshComponent mMesh;

        private bool mLandingScriptResolved = false;

        void OnCreate()
        {
            mInfoPanel = this.FindParentEntity(this.ID);
            mStarship = mInfoPanel.FindParentEntity(mInfoPanel.ID);
            mMesh = mStarship.GetComponent<MeshComponent>();
        }

        void OnEvent()
        {
            if (mLandingScriptResolved)
            {
                mMesh.PlayAnimation("UnloadCargo");
                mStarshipLandingInstance.SetCargoState(CargoState.UnloadingCargo);
            }
        }

        void OnUpdate(float ts)
        {
            if (!mLandingScriptResolved)
            {
                mStarshipScriptHandle = mStarship.GetComponent<ScriptComponent>().ScriptInstance;

                GCHandle gch = GCHandle.FromIntPtr(mStarshipScriptHandle);

                mStarshipLandingInstance = gch.Target as StarshipLanding;
                if (mStarshipLandingInstance == null)
                    throw new Exception("Retrieved script instance is not of type Camera");

                mLandingScriptResolved = true;
            }
        }
    }
}