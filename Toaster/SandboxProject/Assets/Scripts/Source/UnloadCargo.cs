using Sandbox.Source;
using System;
using System.Diagnostics.Eventing.Reader;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace Sandbox
{
    public class UnloadCargo : Entity
    {
        // Tunables
        public float RampApproachDistance = 3.0f;   // forward onto the ramp
        public float SlopeDistance = 5.0f;   // down the slope
        public float ClearDistance = 3.0f;   // forward off the ramp
        public float SlopeAngle = 20.0f;  // degrees nose-down
        public float TranslationSpeed = 1.0f;
        public float RotationSpeed = 30.0f;

        private Entity mInfoPanel;
        private Entity mStarship;
        private IntPtr mStarshipScriptHandle;
        private StarshipLanding mStarshipLandingInstance;
        private MeshComponent mMesh;

        private Entity mCargoOne;
        private TransformComponent mCargoOneTC;
        private Entity mCargoTwo;
        private TransformComponent mCargoTwoTC;
        private MoveableComponent mCargoOneMC;
        private MoveableComponent mCargoTwoMC;

        private bool mLandingScriptResolved = false;

        private enum UnloadingStep { Idle, AnimationPlaying, ToRamp, RotateDown, DownSlope, RotateUp, Clear, Detach, Done }
        private UnloadingStep mUnloadingSequence = UnloadingStep.Idle;

        void OnCreate()
        {
            mInfoPanel = this.FindParentEntity(this.ID);
            mStarship = mInfoPanel.FindParentEntity(mInfoPanel.ID);
            mMesh = mStarship.GetComponent<MeshComponent>();

            mCargoOne = this.FindDecententByName(mStarship.ID, "Rover1");
            mCargoOneTC = mCargoOne.GetComponent<TransformComponent>();
            mCargoTwo = this.FindDecententByName(mStarship.ID, "Rover2");
            mCargoTwoTC = mCargoTwo.GetComponent<TransformComponent>();
            mCargoOneMC = mCargoOne.GetComponent<MoveableComponent>();
            mCargoTwoMC = mCargoTwo.GetComponent<MoveableComponent>();
        }

        void OnEvent()
        {
            if (mLandingScriptResolved)
            {
                mMesh.PlayAnimation("UnloadCargo");
                mStarshipLandingInstance.SetCargoState(CargoState.UnloadingCargo);
                mUnloadingSequence = UnloadingStep.AnimationPlaying;
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

            switch (mUnloadingSequence)
            {
                case UnloadingStep.AnimationPlaying:
                    if (mMesh.IsAnimationComplete("UnloadCargo"))
                    {
                        mCargoOneTC.TranslationSpeed = TranslationSpeed;
                        mCargoTwoTC.TranslationSpeed = TranslationSpeed;
                        Vector3 forwardR1 = mCargoOneTC.WorldRight;
                        Vector3 forwardR2 = mCargoTwoTC.WorldRight;
                        Vector3 currentR1 = mCargoOneTC.Translation;
                        Vector3 currentR2 = mCargoTwoTC.Translation;
                        mCargoOneTC.SetTargetTranslation(currentR1 + forwardR1 * RampApproachDistance);
                        mCargoTwoTC.SetTargetTranslation(currentR2 + forwardR2 * RampApproachDistance);
                        mUnloadingSequence = UnloadingStep.ToRamp;
                    }
                    break;

                case UnloadingStep.ToRamp:
                    if (mCargoOneTC.HasReachedTargetTranslation() && mCargoTwoTC.HasReachedTargetTranslation())
                    {
                        mCargoOneTC.AngularSpeed = RotationSpeed;
                        mCargoTwoTC.AngularSpeed = RotationSpeed;
                        mCargoOneTC.SetTargetRotationDelta(0.0f, 0.0f, -SlopeAngle);
                        mCargoTwoTC.SetTargetRotationDelta(0.0f, 0.0f, -SlopeAngle);
                        mUnloadingSequence = UnloadingStep.RotateDown;
                    }
                    break;

                case UnloadingStep.RotateDown:
                    if (mCargoOneTC.HasReachedTargetRotation(0.01f) && mCargoTwoTC.HasReachedTargetRotation(0.01f))
                    {
                        Vector3 slopeDirR1 = mCargoOneTC.WorldRight;
                        Vector3 slopeDirR2 = mCargoTwoTC.WorldRight;
                        Vector3 currentR1 = mCargoOneTC.Translation;
                        Vector3 currentR2 = mCargoTwoTC.Translation;
                        mCargoOneTC.SetTargetTranslation(currentR1 + slopeDirR1 * SlopeDistance);
                        mCargoTwoTC.SetTargetTranslation(currentR2 + slopeDirR2 * SlopeDistance);
                        mUnloadingSequence = UnloadingStep.DownSlope;
                    }
                    break;

                case UnloadingStep.DownSlope:
                    if (mCargoOneTC.HasReachedTargetTranslation() && mCargoTwoTC.HasReachedTargetTranslation())
                    {
                        mCargoOneTC.SetTargetRotationDelta(0.0f, 0.0f, +SlopeAngle);
                        mCargoTwoTC.SetTargetRotationDelta(0.0f, 0.0f, +SlopeAngle);
                        mUnloadingSequence = UnloadingStep.RotateUp;
                    }
                    break;

                case UnloadingStep.RotateUp:
                    if (mCargoOneTC.HasReachedTargetRotation(0.01f) && mCargoTwoTC.HasReachedTargetRotation(0.01f))
                    {
                        Vector3 forwardR1 = mCargoOneTC.WorldRight;   // now level, forward is horizontal again
                        Vector3 forwardR2 = mCargoTwoTC.WorldRight;
                        Vector3 currentR1 = mCargoOneTC.Translation;
                        Vector3 currentR2 = mCargoTwoTC.Translation;
                        mCargoOneTC.SetTargetTranslation(currentR1 + forwardR1 * ClearDistance);
                        mCargoTwoTC.SetTargetTranslation(currentR2 + forwardR2 * ClearDistance);
                        mUnloadingSequence = UnloadingStep.Clear;
                    }
                    break;

                case UnloadingStep.Clear:
                    if (!mCargoOneTC.IsTranslating() && !mCargoTwoTC.IsTranslating())
                    {
                        mCargoOne.Unparent();
                        mCargoTwo.Unparent();
                        mCargoOneMC.IsActive = true;   // now accepts MoveTo
                        mCargoTwoMC.IsActive = true;   // now accepts MoveTo
                        Toast.Console.LogWarning("After unparent: IsTranslating=" + mCargoOneTC.IsTranslating() + " pos=" + mCargoOneTC.Translation);
                        mUnloadingSequence = UnloadingStep.Done;
                    }
                    break;
            }
        }
    }
}