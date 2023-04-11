using System;
using Toast;

namespace Sandbox
{
    public class AnimationTest : Entity
    {
        private MeshComponent mComponent;

        void OnCreate()
        {
            mComponent = FindEntityByName("Starship").GetComponent<MeshComponent>();
        }

        void OnUpdate(float ts)
        {
            
        }

        void OnEvent()
        {
            float timeElapsed = mComponent.StopAnimation("LandingLegs_Undeploy");
            float startTime = timeElapsed > 0.0f ? (mComponent.GetDurationAnimation("LandingLegs_Deploy") - timeElapsed) : 0.0f;
            mComponent.PlayAnimation("LandingLegs_Deploy", startTime);
        }
    }
}