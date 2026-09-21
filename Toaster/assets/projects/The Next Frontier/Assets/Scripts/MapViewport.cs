using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace TheNextFrontier
{
    public class MapViewport : Entity
    {
    	public float ZoomSpeed = 1.5f;
    	public float MinWidth = 0.02f;
    	
    	private UIImageComponent mImage;
    	
    	private Vector4 mHomeRect;//
    	
    	private bool mPanning;
    	private Vector2 mPanLastLocal;
    	
        void OnCreate()
        {
        	mImage = this.GetComponent<UIImageComponent>();
        	
        	mHomeRect = ComputeHomeRect();
        	mImage.SourceRect = mHomeRect;
        }

        bool OnEvent(Event e)
        {
        	if(e.Type != EventType.MouseScrolled)
        		return HandlePanEvent(e);
        	
			if(!mImage.GetLocalCursorPos(out Vector2 anchor))
				return false;
			
			Zoom(e.ScrollDelta, anchor);	
			
			return true;
        }

        void OnUpdate(float ts)
        {
	        if(!mPanning)
	        	return;
	        
	        // Release is polled
	        if(!Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
	        {
	        	mPanning = false;
	        	return;
	        }
	        
	        mImage.GetLocalCursorPos(out Vector2 local);
	        
	        Vector2 delta = local - mPanLastLocal;
	        mPanLastLocal = local;
	        
	        Vector4 r = mImage.SourceRect;
	        r.X -= delta.X * r.Z;
	        r.Y -= delta.Y * r.W;
	        
	        mImage.SourceRect = ClampInsideTexture(r);
        }
        
        private Vector4 ComputeHomeRect()
        {
			Vector2 texSize = mImage.TextureSize;
			Vector3 scale = this.GetComponent<TransformComponent>().Scale;
			
			if(texSize.X <= 0.0f || texSize.Y <= 0.0f || scale.X <= 0.0f || scale.Y <= 0.0f)
				return new Vector4(0.0f, 0.0f, 1.0f, 1.0f);
				
			float elementAspect = scale.X / scale.Y;
			float textureAspect = texSize.X / texSize.Y;
			float ratio = elementAspect / textureAspect;
			
			float w, h;
			if(ratio <= 1.0f)
			{
				h = 1.0f;
				w = ratio;
			}
			else
			{
				w = 1.0f; 
				h = 1.0f / ratio;				
			}
			
			return new Vector4((1.0f - w) * 0.5f, (1.0f - h) * 0.5f, w, h);
        }
        
        private void Zoom(float notches, Vector2 anchor)
        {
        	Vector4 r = mImage.SourceRect;
        	
        	float factor = (float)Math.Pow(ZoomSpeed, -notches);
        	
        	float newW = Math.Clamp(r.Z * factor, MinWidth, mHomeRect.Z);
        	float newH = newW * (r.W / r.Z);
        	
        	r.X += (r.Z - newW) * anchor.X;
            r.Y += (r.W - newH) * anchor.Y;
            r.Z = newW;
            r.W = newH;

            mImage.SourceRect = ClampInsideTexture(r);
        }
        
        private bool HandlePanEvent(Event e)
        {
        	if(e.Type == EventType.MouseButtonPressed && (MouseCode)e.MouseButton == MouseCode.ButtonLeft)
        	{
				if(mImage.GetLocalCursorPos(out mPanLastLocal))
				{
					mPanning = true;
					return true;
				}
        	}
        	
        	return false;
        }
        
      	private Vector4 ClampInsideTexture(Vector4 r)
        {
            r.X = Math.Clamp(r.X, 0.0f, 1.0f - r.Z);
            r.Y = Math.Clamp(r.Y, 0.0f, 1.0f - r.W);
            return r;
        }
    }
}
