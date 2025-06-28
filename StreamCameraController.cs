using Unity.Collections;
using UnityEngine;
using UnityEngine.Rendering;

using System;
using System.IO;
using System.Threading;

public class FrameUpdatedEvent : EventArgs
{
    public Color32[] Data = { }; // Raw pixel data
    public int Width { get; set; }
    public int Height { get; set; }
}

public class StreamCameraController : MonoBehaviour
{
    public float FrameInterval = 1f / 10; // Desired frames per second for streaming (e.g., 0.1 for 10 FPS)
    public int Width = 720; // Resolution width for the stream
    public int Height = 720; // Resolution height for the stream

    private Camera _streamCamera; // Reference to the camera component on this GameObject
    private VideoServer _videoServer; // Reference to the video server instance
    private float _lastFrameAt = 0; // Timestamp of the last frame sent
    private float _lastFrameRenderTime = 0; // Time taken to render the last frame
    private RenderTexture _renderTexture; // Render texture to capture camera output
    private Texture2D _tempTexture; // Temporary texture to read pixels into

    // Event for when a new frame is updated and ready to be streamed
    public EventHandler<FrameUpdatedEvent> FrameUpdated;

    // Sets the camera's position and rotation based on an offset array.
    public void SetCameraOffset(float[] offset) {
        var initialCameraPosition = new Vector3(0, 1.8f, 0); // Base position for the camera
        transform.position = initialCameraPosition + new Vector3(
            getOffset(offset, 0), // X offset
            getOffset(offset, 1), // Y offset
            getOffset(offset, 2)  // Z offset
        );
        // Set camera rotation (Pitch, Yaw, Roll) relative to a base rotation
        transform.eulerAngles = new Vector3(
            90 + getOffset(offset, 3), // Pitch (rotation around X-axis)
            270 + getOffset(offset, 4), // Yaw (rotation around Y-axis)
            getOffset(offset, 5)        // Roll (rotation around Z-axis)
        );
    }

    // Starts the video server for streaming.
    public void StartVideoServer(int port)
    {
        _streamCamera = this.GetComponent<Camera>(); // Get the camera component
        if (_streamCamera == null)
        {
            Debug.LogError("StreamCameraController: No Camera component found on this GameObject.", this);
            return;
        }

        // Create a new render texture for the camera to render into
        _renderTexture = new RenderTexture(Width, Height, 16, RenderTextureFormat.ARGB32);
        _renderTexture.Create();
        _streamCamera.targetTexture = _renderTexture; // Set camera's target to the render texture

        // Create a temporary Texture2D to read pixels from the render texture
        _tempTexture = new Texture2D(Width, Height, TextureFormat.RGBA32, false);

        // Initialize and start the VideoServer
        _videoServer = new VideoServer();
        _videoServer.Start(port, this); // Pass 'this' (StreamCameraController) to the VideoServer
    }

    // Safely gets an offset value from an array, returning 0 if index is out of bounds.
    private float getOffset(float[] offset, int index) {
        if (offset != null && offset.Length > index) {
            return offset[index];
        }
        return 0f;
    }

    // Invokes the FrameUpdated event with the new frame data.
    private void OnFrameUpdated(Color32[] frameData)
    {
        // Only invoke if there are subscribers to the event
        if (FrameUpdated != null)
        {
            FrameUpdated.Invoke(this, new FrameUpdatedEvent { Data = frameData, Width = Width, Height = Height });
        }
    }

    // Called automatically by Unity after the camera finishes rendering.
    void OnRenderImage(RenderTexture source, RenderTexture destination)
    {
        // Check if video server is running and if it's time to send a new frame
        if (_videoServer != null && Time.realtimeSinceStartup - _lastFrameAt >= FrameInterval - _lastFrameRenderTime / 2)
        {
            _lastFrameAt = Time.realtimeSinceStartup; // Update last frame timestamp
            RenderTexture.active = _renderTexture; // Set the render texture as active for reading
            _tempTexture.ReadPixels(new Rect(0, 0, Width, Height), 0, 0, false); // Read pixels from render texture
            _tempTexture.Apply(); // Apply pixel changes to the texture
            OnFrameUpdated(_tempTexture.GetPixels32()); // Fire the event with the new frame data
            RenderTexture.active = null; // Unset the active render texture
            
            _lastFrameRenderTime = Time.realtimeSinceStartup - _lastFrameAt; // Calculate time taken to render
        }
        // IMPORTANT: Always blit the source to destination, regardless of whether a frame was captured for streaming.
        // This ensures the camera output is always displayed on screen.
        Graphics.Blit(source, destination);
    }
}
