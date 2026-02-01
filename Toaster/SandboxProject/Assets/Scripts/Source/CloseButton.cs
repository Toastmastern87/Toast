using System;
using Toast;

namespace Sandbox
{
    public class CloseButton : Entity
    {
        private Entity mStarship;

        private Entity mStarshipInfoPanel;
        private UIPanelComponent mPanel;

        void OnCreate()
        {
            mStarshipInfoPanel = this.FindParentEntity(this.ID);
            mPanel = mStarshipInfoPanel.GetComponent<UIPanelComponent>();
        }

        void OnEvent()
        {
            mPanel.Visible = false;
        }

        void OnUpdate(float ts)
        {
        }
    }
}