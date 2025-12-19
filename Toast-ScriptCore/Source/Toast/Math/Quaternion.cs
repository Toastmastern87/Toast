using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Source.Toast.Math
{
    public struct Quaternion
    {
        public float X, Y, Z, W;

        public Quaternion(float x, float y, float z, float w)
        { 
            X = x;
            Y = y; 
            Z = z;
            W = w; 
        }

        public static Quaternion Identity => new Quaternion(0, 0, 0, 1);
    }
}
