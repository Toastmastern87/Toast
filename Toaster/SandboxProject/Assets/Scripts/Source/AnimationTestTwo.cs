using System;
using Toast;

namespace Sandbox
{
    public class AnimationTestTwo : Entity
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
            float timeElapsed = mComponent.StopAnimation("LandingLegs_Deploy");
            float startTime = timeElapsed > 0.0f ? (mComponent.GetDurationAnimation("LandingLegs_Undeploy") - timeElapsed) : 0.0f;
            mComponent.PlayAnimation("LandingLegs_Undeploy", startTime);
        }
    }
}