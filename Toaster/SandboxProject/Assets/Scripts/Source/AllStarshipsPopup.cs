using System;
using System.Runtime.InteropServices;

using Toast;

namespace Sandbox
{
    public class AllStarshipsPopup : Entity
    {
        private UIPanelComponent mPanel;
        private UITextComponent mText;

        void OnCreate()
        {
            mPanel = FindEntityByName("AllStarshipsPopup").GetComponent<UIPanelComponent>();
            mText = FindEntityByName("AllStarshipText").GetComponent<UITextComponent>(); 
        }

        void OnEvent()
        {         
            mPanel.Visible = true;
        }

        void OnUpdate(float ts)
        {
            mText.Text = "";

            Entity[] starshipIDs = Scene.GetPrefabEntities("Starship");

            for (int i = starshipIDs.Length-1; i >= 0; i--)
            {
                string starshipName = starshipIDs[i].GetComponent<TagComponent>().Tag;

                mText.Text += starshipName + "\n";
            }
        }

    }
}
