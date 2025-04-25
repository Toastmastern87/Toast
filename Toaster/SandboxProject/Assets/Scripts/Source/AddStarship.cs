using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace Sandbox
{
    public class AddStarship : Entity
    {
        private Entity mMoney;
        private IntPtr mMoneyScriptHandle;
        private Money mMoneyInstance;

        private int mNumberOfStarships;

        void OnCreate()
        {
            mMoney = FindEntityByName("Money");
            mMoneyScriptHandle = mMoney.GetComponent<ScriptComponent>().ScriptInstance;

            GCHandle gch = GCHandle.FromIntPtr(mMoneyScriptHandle);

            mMoneyInstance = gch.Target as Money;
            if (mMoneyInstance == null)
                throw new Exception("Retrieved script instance is not of type Money");

            mNumberOfStarships = 0;
        }

        void OnEvent()
        {
            bool starshipAdded = mMoneyInstance.SetRetracttMoney(50000);

            if (starshipAdded) 
            {
                mNumberOfStarships++;

                Entity newStarship = Scene.AddPrefab("Starship");

                newStarship.GetComponent<TagComponent>().Tag = "Starship " + mNumberOfStarships;
                newStarship.GetComponent<TransformComponent>().Translation = new Vector3(-9.0f + 10.0f * mNumberOfStarships, 50.0f, 291.0f);

            }
        }

        void OnUpdate(float ts)
        {
             
        }

        public void SetTargetMoney(int newMoney)
        {
           
        }
    }
}
