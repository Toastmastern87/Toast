using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Toast;

class ClientHelloWorld : Entity
{
    private TagComponent mTag;

    void OnCreate()
    {
        mTag = GetComponent<TagComponent>();

        Toast.Console.LogInfo("Creating Entity: " + mTag.Tag);
    }

    void OnUpdate(float ts)
    {
        mTag = GetComponent<TagComponent>();

        Toast.Console.LogTrace("Updating " + mTag.Tag + ", timestep: " + ts);
    }
}
