//using System;

//using Toast;

//class GameSpeedButtons : EntityOld
//{
//    private UIButtonComponent mButton1, mButton2, mButton3, mButton4;

//    private string mName;
//    private EntityOld mButton1Entity, mButton2Entity, mButton3Entity, mButton4Entity;

//    void OnCreate()
//    {
//        mButton1Entity = FindEntityByTag("TimeScale1");
//        mButton2Entity = FindEntityByTag("TimeScale2");
//        mButton3Entity = FindEntityByTag("TimeScale3");
//        mButton4Entity = FindEntityByTag("TimeScale4");

//        mButton1 = mButton1Entity.GetComponent<UIButtonComponent>();
//        mButton2 = mButton2Entity.GetComponent<UIButtonComponent>();
//        mButton3 = mButton3Entity.GetComponent<UIButtonComponent>();
//        mButton4 = mButton4Entity.GetComponent<UIButtonComponent>();
//    }

//    void OnUpdate(float ts)
//    {
//    }

//    void OnClick() 
//    {
//        mName = GetComponent<TagComponent>().Tag;

//        if (mName == "TimeScale1") 
//        {
//            mButton4.Color = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);
//            mButton3.Color = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);
//            mButton2.Color = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);

//            Scene.TimeScale = 1.0f;
//        }

//        if (mName == "TimeScale2") 
//        {
//            mButton4.Color = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);
//            mButton3.Color = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);

//            if (mButton2.Color == new Vector4(1.0f, 1.0f, 1.0f, 1.0f))
//                mButton2.Color = new Vector4(0.0f, 0.0f, 0.0f, 1.0f);

//            Scene.TimeScale = 2.0f;
//        }

//        if (mName == "TimeScale3")
//        {
//            mButton4.Color = new Vector4(1.0f, 1.0f, 1.0f, 1.0f);

//            if (mButton3.Color == new Vector4(1.0f, 1.0f, 1.0f, 1.0f))
//                mButton3.Color = new Vector4(0.0f, 0.0f, 0.0f, 1.0f);

//            mButton2.Color = new Vector4(0.0f, 0.0f, 0.0f, 1.0f);

//            Scene.TimeScale = 4.0f;
//        }

//        if (mName == "TimeScale4")
//        {
//            if (mButton4.Color == new Vector4(1.0f, 1.0f, 1.0f, 1.0f))
//                mButton4.Color = new Vector4(0.0f, 0.0f, 0.0f, 1.0f);

//            mButton3.Color = new Vector4(0.0f, 0.0f, 0.0f, 1.0f);
//            mButton2.Color = new Vector4(0.0f, 0.0f, 0.0f, 1.0f);

//            Scene.TimeScale = 32.0f;
//        }
//    }
//}