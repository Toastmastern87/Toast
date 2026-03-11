using System;
using Toast;

namespace Sandbox
{
    public class ThrustVectorControllerTest : Entity
    {
        private TransformComponent mRaptorSea1TC;
        private TransformComponent mRaptorSea2TC;
        private TransformComponent mRaptorSea3TC;
        private int animationStepRS1;
        private int animationStepRS2;
        private int animationStepRS3;

        void OnCreate()
        {
            animationStepRS1 = 0;
            animationStepRS2 = 0;
            animationStepRS3 = 0;

            mRaptorSea1TC = FindChildEntityByName(this.Name, "RaptorSea1").GetComponent<TransformComponent>();
            mRaptorSea2TC = FindChildEntityByName(this.Name, "RaptorSea2").GetComponent<TransformComponent>();
            mRaptorSea3TC = FindChildEntityByName(this.Name, "RaptorSea3").GetComponent<TransformComponent>();
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            switch (animationStepRS1)
            {
                case 0:
                    mRaptorSea1TC.AngularSpeed = 45.0f; // 45 deg/sec
                    mRaptorSea1TC.SetTargetRotation(0.0f, 0.0f, -15.0f);
                    animationStepRS1 = 1;
                    break;
                case 1: // Wait for yaw to finish, then pitch
                    if (mRaptorSea1TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea1TC.SetTargetRotation(15.0f, 0.0f, -15.0f);
                        animationStepRS1 = 2;
                    }
                    break;
                case 2: // Wait for pitch, then return to neutral
                    if (mRaptorSea1TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea1TC.SetTargetRotation(0.0f, 0.0f, 0.0f);
                        animationStepRS1 = 3;
                    }
                    break;
                case 3: // Done
                    if (mRaptorSea1TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea1TC.IsRotating = false;
                        mRaptorSea1TC.AngularSpeed = 0.0f;
                        animationStepRS1 = 0;
                    }
                    break;
            }

            switch (animationStepRS2)
            {
                case 0:
                    mRaptorSea2TC.AngularSpeed = 45.0f; // 45 deg/sec
                    mRaptorSea2TC.SetTargetRotation(0.0f, 0.0f, -15.0f);
                    animationStepRS2 = 1;
                    break;
                case 1: // Wait for yaw to finish, then pitch
                    if (mRaptorSea2TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea2TC.SetTargetRotation(15.0f, 0.0f, -15.0f);
                        animationStepRS2 = 2;
                    }
                    break;
                case 2: // Wait for pitch, then return to neutral
                    if (mRaptorSea2TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea2TC.SetTargetRotation(0.0f, 0.0f, 0.0f);
                        animationStepRS2 = 3;
                    }
                    break;
                case 3: // Done
                    if (mRaptorSea2TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea2TC.IsRotating = false;
                        mRaptorSea2TC.AngularSpeed = 0.0f;
                        animationStepRS2 = 0;
                    }
                    break;
            }

            switch (animationStepRS3)
            {
                case 0:
                    mRaptorSea3TC.AngularSpeed = 45.0f; // 45 deg/sec
                    mRaptorSea3TC.SetTargetRotation(0.0f, 0.0f, -15.0f);
                    animationStepRS3 = 1;
                    break;
                case 1: // Wait for yaw to finish, then pitch
                    if (mRaptorSea3TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea3TC.SetTargetRotation(15.0f, 0.0f, -15.0f);
                        animationStepRS3 = 2;
                    }
                    break;
                case 2: // Wait for pitch, then return to neutral
                    if (mRaptorSea3TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea3TC.SetTargetRotation(0.0f, 0.0f, 0.0f);
                        animationStepRS3 = 3;
                    }
                    break;
                case 3: // Done
                    if (mRaptorSea3TC.HasReachedTargetRotation(0.1f))
                    {
                        mRaptorSea3TC.IsRotating = false;
                        mRaptorSea3TC.AngularSpeed = 0.0f;
                        animationStepRS3 = 0;
                    }
                    break;
            }
        }
    }
}