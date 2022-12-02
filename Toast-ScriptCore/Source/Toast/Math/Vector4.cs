using System;
using System.Runtime.InteropServices;

namespace Toast
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4
    {
        public float X;
        public float Y;
        public float Z;
        public float W;

        public Vector4(float scalar)
        {
            X = Y = Z = W = scalar;
        }

        public Vector4(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public override string ToString()
        {
            return "Vector4[" + X + ", " + Y + ", " + Z + ", " + W + "]";
        }

        public static float Length(Vector4 vec)
        {
            return (float)Math.Sqrt((vec.X * vec.X) + (vec.Y * vec.Y) + (vec.Z * vec.Z) + (vec.W * vec.W));
        }

        public static Vector4 Normalize(Vector4 vec)
        {
            return new Vector4(vec.X / Length(vec), vec.Y / Length(vec), vec.Z / Length(vec), vec.W / Length(vec));
        }

        public static float Dot(Vector4 vecOne, Vector4 vecTwo)
        {
            return vecOne.X * vecTwo.X + vecOne.Y * vecTwo.Y + vecOne.Z * vecTwo.Z + vecOne.W * vecTwo.W;
        }

        public static double AngleNormalizedVectors(Vector4 vecOne, Vector4 vecTwo)
        {
            double dot = Dot(vecOne, vecTwo);
            double lengthVecOne = Length(vecOne);
            double lengthVecTwo = Length(vecTwo);

            if (dot > 1)
                dot = 1.0f;

            return Math.Acos(dot);
        }

        public static bool operator ==(Vector4 vecOne, Vector4 vecTwo)
        {
            if (vecOne.X == vecTwo.X && vecOne.Y == vecTwo.Y && vecOne.Z == vecTwo.Z && vecOne.W == vecTwo.W)
                return true;
            else
                return false;
        }

        public static bool operator !=(Vector4 vecOne, Vector4 vecTwo)
        {
            return !(vecOne == vecTwo);
        }
    }
}
