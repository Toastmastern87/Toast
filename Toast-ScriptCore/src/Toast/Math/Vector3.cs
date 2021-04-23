//using System;
//using System.Collections.Generic;
//using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
//using System.Linq;
//using System.Text;
//using System.Threading.Tasks;

namespace Toast
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float X;
        public float Y;
        public float Z;

        public Vector3(float scalar) 
        {
            X = Y = Z = scalar;
        }

        public Vector3(float x, float y, float z) 
        {
            X = x;
            Y = y;
            Z = z;
        }

        public override string ToString()
        {
            return "Vector3[" + X + ", " + Y + ", " + Z + "]";
        }

        public static Vector3 operator +(Vector3 vectorOne, Vector3 vectorTwo) 
        {
            return new Vector3(vectorOne.X + vectorTwo.X, vectorOne.Y + vectorTwo.Y, vectorOne.Z + vectorTwo.Z);
        }

        public static Vector3 operator +(Vector3 vectorOne, float value)
        {
            return new Vector3(vectorOne.X + value, vectorOne.Y + value, vectorOne.Z + value);
        }
    }
}
