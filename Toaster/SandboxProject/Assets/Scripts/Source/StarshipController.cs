using Source.Toast.Math;
using System;
using System.IO;
using Toast;

namespace Sandbox
{
    public class StarshipController : Entity
    {
        private Entity mStarship;
        private Entity mRB1;
        private Entity mRB2;
        private Entity mRB3;
        private TransformComponent mTransform;
        private RigidBodyComponent mRigidBody;
        private BoxColliderComponent mBoxCollider;

        private ParticlesComponent mRB1Particles;
        private ParticlesComponent mRB2Particles;
        private ParticlesComponent mRB3Particles;

        private bool mShutdownEngines = false;
        private bool mEnginesRunning = false;

        private Entity mStarshipInfoPanel;
        private Entity mVelocityEntity;
        private Entity mAltitudeEntity;

        private TransformComponent mPanelTransform;
        private UIPanelComponent mPanel;
        private UITextComponent mVelocityText;
        private UITextComponent mAltitudeText;

        private Vector2 mPreviousPos;
        private bool mIsDragging;

        private float mTotalTime;

        void OnCreate()
        {
            mStarship = this; 
            mRB1 = FindChildEntityByName(this.Name, "Rocket Exhaust RB1");
            mRB2 = FindChildEntityByName(this.Name, "Rocket Exhaust RB2");
            mRB3 = FindChildEntityByName(this.Name, "Rocket Exhaust RB3");
            mTransform = mStarship.GetComponent<TransformComponent>();
            mRigidBody = mStarship.GetComponent<RigidBodyComponent>();
            mBoxCollider = mStarship.GetComponent<BoxColliderComponent>();

            mRB1Particles = mRB1.GetComponent<ParticlesComponent>();
            mRB2Particles = mRB2.GetComponent<ParticlesComponent>();
            mRB3Particles = mRB3.GetComponent<ParticlesComponent>();

            mTotalTime = 0.0f;

            mStarshipInfoPanel = FindChildEntityByName(mStarship.GetComponent<TagComponent>().Tag, "InfoPopup");
            mVelocityEntity = mStarshipInfoPanel.FindDecententByName(mStarshipInfoPanel.ID, "VelocityText");
            mAltitudeEntity = mStarshipInfoPanel.FindDecententByName(mStarshipInfoPanel.ID, "AltitudeText");
            mPanel = mStarshipInfoPanel.GetComponent<UIPanelComponent>();
            mPanelTransform = mStarshipInfoPanel.GetComponent<TransformComponent>();
            mVelocityText = mVelocityEntity.GetComponent<UITextComponent>();
            mAltitudeText = mAltitudeEntity.GetComponent<UITextComponent>();

            bool mIsDragging = false;
        }

        void OnEvent()
        {
            if (Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
                mPanel.Visible = true;
        }

        void OnUpdate(float ts)
        {
            float altitude = mBoxCollider.Altitude;

            Vector3 v = mRigidBody.LinearVelocity;

            const float groundAltEps = 0.5f;
            const float stopSpeedEps = 0.25f;

            if (altitude <= 90.0f && altitude > groundAltEps && !mShutdownEngines)
                mEnginesRunning = true;

            if (mEnginesRunning && altitude <= groundAltEps && Vector3.Length(v) <= stopSpeedEps)
            {
                mEnginesRunning = false;
                mShutdownEngines = true;
            }

            mRB1Particles.Emitting = mEnginesRunning;
            mRB2Particles.Emitting = mEnginesRunning;
            mRB3Particles.Emitting = mEnginesRunning;

            if (mEnginesRunning)
            {
                Quaternion totalRot = mTransform.Rotation; // or compose Euler+Quat if you expose both
                Vector3 thrustDirWS = Vector3.Normalize(Vector3.Rotate(totalRot, new Vector3(0.0f, 1.0f, 0.0f)));

                float thrustForceMagnitude = 38413.0f; // example from your current scenario
                Vector3 thrustForce = thrustDirWS * thrustForceMagnitude; // N

                Vector3 impulse = thrustForce * ts; // kg·m/s
                PhysicsEngine.ApplyLinearImpulse(this.ID, impulse);
            }

            if (mPanel.Visible)
            {
                Vector2 mousePos = Input.GetMousePosition();

                if (Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
                {

                    if (!mIsDragging)
                    {
                        Vector3 panelTranslation = mPanelTransform.Translation;
                        Vector2 viewportSize = Scene.RenderTargetSize();

                        panelTranslation.X += (viewportSize.X * 0.5f);
                        panelTranslation.Y += (viewportSize.Y * 0.5f);

                        Vector3 panelScale = mPanelTransform.Scale;
                        float borderSize = 20.0f;

                        // Check if the mouse is over the panel to start dragging
                        if ((mousePos.X >= panelTranslation.X && mousePos.X <= (panelTranslation.X + panelScale.X)) && (mousePos.Y <= (panelTranslation.Y + borderSize) && mousePos.Y >= panelTranslation.Y))
                        {
                            mIsDragging = true;
                            mPreviousPos = mousePos;
                        }
                    }
                    else
                    {
                        // Calculate the delta movement
                        Vector2 deltaMousePos = mousePos - mPreviousPos;

                        // Update the panel's translation
                        mPanelTransform.Translation += new Vector3(deltaMousePos.X, deltaMousePos.Y, 0.0f);

                        // Update the previous mouse position
                        mPreviousPos = mousePos;
                    }
                }
                else
                    mIsDragging = false;

                if (mTotalTime > 0.15f)
                {
                    float linearVelocity = Vector3.Length(mRigidBody.LinearVelocity);

                    mVelocityText.Text = $"Velocity: {linearVelocity:F1} m/s";
                    mAltitudeText.Text = $"Altitude: {altitude:F1} m";
                }

                if (mTotalTime > 0.2f)
                    mTotalTime -= 0.2f;

                mTotalTime += ts;
            }
        }
    }
}
