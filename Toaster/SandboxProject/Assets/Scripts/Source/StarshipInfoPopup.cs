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

        private TransformComponent mPanelTransform;
        private UIPanelComponent mPanel;
        private RigidBodyComponent mRigidBody;
        private UITextComponent mVelocityText;

        private Vector2 mPreviousPos;
        private bool mIsDragging;

        void OnCreate()
        {
            mStarship = FindEntityByName("Starship SN3");
            mStarshipInfoPanel = FindChildEntityByName("Starship SN3", "InfoPopup");
            mVelocityEntity = FindChildEntityByName("InfoPopup", "VelocityText");
            mPanel = mStarshipInfoPanel.GetComponent<UIPanelComponent>();
            mPanelTransform = mStarshipInfoPanel.GetComponent<TransformComponent>();
            mRigidBody = mStarship.GetComponent<RigidBodyComponent>();
            mVelocityText = mVelocityEntity.GetComponent<UITextComponent>();

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

                float linearVelocity = Vector3.Length(mRigidBody.LinearVelocity);

                mVelocityText.Text = "Velocity: " + (float)Math.Round(linearVelocity, 0) + " m/s";
            }
        }
    }
}