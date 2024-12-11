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

        private UIPanelComponent mPanel;
        private RigidBodyComponent mRigidBody;
        private UITextComponent mVelocityText;

        void OnCreate()
        {
            mStarship = FindEntityByName("Starship SN3");
            mStarshipInfoPanel = FindChildEntityByName("Starship SN3", "InfoPopup");
            mVelocityEntity = FindChildEntityByName("InfoPopup", "VelocityText");
            mPanel = mStarshipInfoPanel.GetComponent<UIPanelComponent>();
            mRigidBody = mStarship.GetComponent<RigidBodyComponent>();
            mVelocityText = mVelocityEntity.GetComponent<UITextComponent>();
        }

        void OnEvent()
        {
            mPanel.Visible = true;
        }

        void OnUpdate(float ts)
        {
            if (mPanel.Visible == true)
            {
                float linearVelocity = Vector3.Length(mRigidBody.LinearVelocity);

                mVelocityText.Text = "Velocity: " + (float)Math.Round(linearVelocity, 0) + " m/s";
            }
        }
    }
}