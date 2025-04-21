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

        void OnCreate()
        {
            mMoney = FindEntityByName("Money");
            mMoneyScriptHandle = mMoney.GetComponent<ScriptComponent>().ScriptInstance;

            GCHandle gch = GCHandle.FromIntPtr(mMoneyScriptHandle);

            mMoneyInstance = gch.Target as Money;
            if (mMoneyInstance == null)
                throw new Exception("Retrieved script instance is not of type Money");
        }

        void OnEvent()
        {
            bool starshipAdded = mMoneyInstance.SetRetracttMoney(50000);

            if (starshipAdded)
                Scene.AddPrefab("Starship");
        }

        void OnUpdate(float ts)
        {
             
        }

        public void SetTargetMoney(int newMoney)
        {
           
        }
    }
}
