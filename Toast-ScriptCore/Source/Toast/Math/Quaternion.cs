using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Toast
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

        public static Quaternion Multiply(Quaternion a, Quaternion b)
        {
            return new Quaternion(
                a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
                a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
                a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,
                a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z
            );
        }

        public static Quaternion LookRotation(Vector3 forward, Vector3 up, Vector3 fallbackRight)
        {
            Vector3 f = Vector3.Normalize(forward);

            Vector3 r = Vector3.Cross(up, f);
            if (Vector3.LengthSquared(r) < 1e-6f)
                r = fallbackRight;
            r = Vector3.Normalize(r);

            Vector3 u = Vector3.Cross(f, r);

            float m00 = r.X, m01 = u.X, m02 = f.X;
            float m10 = r.Y, m11 = u.Y, m12 = f.Y;
            float m20 = r.Z, m21 = u.Z, m22 = f.Z;

            float s;
            float trace = m00 + m11 + m22;
            if (trace > 0.0f)
            {
                s = (float)Math.Sqrt(trace + 1.0f) * 2.0f;
                return new Quaternion((m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s);
            }
            if (m00 > m11 && m00 > m22)
            {
                s = (float)Math.Sqrt(1.0f + m00 - m11 - m22) * 2.0f;
                return new Quaternion(0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s);
            }
            if (m11 > m22)
            {
                s = (float)Math.Sqrt(1.0f + m11 - m00 - m22) * 2.0f;
                return new Quaternion((m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s);
            }

            s = (float)Math.Sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            return new Quaternion((m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s);
        }
    }
}
