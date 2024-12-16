using System;
using System.IO;
using Toast;

namespace Sandbox
{
    public class StarshipInfoPopup : Entity
    {
        private Entity mStarship;
        private Entity mStarshipInfoPanel;
        private Entity mVelocityEntity;
        private Entity mAltitudeEntity;

        private TransformComponent mPanelTransform;
        private UIPanelComponent mPanel;
        private RigidBodyComponent mRigidBody;
        private UITextComponent mVelocityText;
        private UITextComponent mAltitudeText;

        private Vector2 mPreviousPos;
        private bool mIsDragging;

        private float mTotalTime;

        void OnCreate()
        {
            mTotalTime = 0.0f;

            mStarship = FindEntityByName("Starship SN3");
            mStarshipInfoPanel = FindChildEntityByName("Starship SN3", "InfoPopup");
            mVelocityEntity = FindChildEntityByName("InfoPopup", "VelocityText");
            mAltitudeEntity = FindChildEntityByName("InfoPopup", "AltitudeText");
            mPanel = mStarshipInfoPanel.GetComponent<UIPanelComponent>();
            mPanelTransform = mStarshipInfoPanel.GetComponent<TransformComponent>();
            mRigidBody = mStarship.GetComponent<RigidBodyComponent>();
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
            if (mPanel.Visible == true)
            {
                Vector2 mousePos = Input.GetMousePosition();

                mRigidBody.ReqAltitude = true;

                if (Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
                {
                    if (!mIsDragging)
                    {
                        Vector3 panelTranslation = mPanelTransform.Translation;
                        Vector3 panelScale = mPanelTransform.Scale;
                        float borderSize = mPanel.BorderSize;

                        // Check if the mouse is over the panel to start dragging
                        if (mousePos.X >= (panelTranslation.X + 1273.0) && mousePos.X <= (panelTranslation.X + 1273.0 + panelScale.X) && mousePos.Y >= (panelTranslation.Y + 517.5 + panelScale.Y - borderSize) && mousePos.Y <= (panelTranslation.Y + 517.5 + panelScale.Y))
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
                    float altitude = mRigidBody.Altitude;

                    mVelocityText.Text = "Velocity: " + (float)Math.Round(linearVelocity, 1) + " m/s";
                    mAltitudeText.Text = "Altitude: " + (float)Math.Round(altitude, 1) + " m";
                }

                if (mTotalTime > 0.2f)
                    mTotalTime -= 0.2f;

                mTotalTime += ts;
            }
            else
                mRigidBody.ReqAltitude = false;
        }
    }
}