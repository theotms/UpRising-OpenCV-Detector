using UnityEngine;
using System.Collections;

/*
    This controller makes sure the objects bounce back instead of going through
    the arena walls when moving too fast for the collisions Unity calculates.
*/
public class DynamicObjectController : Draggable
{
    float _bounceMultiplier = 0.8f;
    Rigidbody _dynamicObject;
    float _arenaMaxX = 1.5f / 2;
    float _arenaMaxY = 2; // Assuming arena floor is Y=0 and objects can go up to Y=2
    float _arenaMaxZ = 1.5f / 2;
    // Collider _collider; // This field is declared but never used. Can be removed if not needed.
    public bool isFlickering = false;
    public bool isGhost = false;
    public int value = 0; // Value awarded when this object enters a goal
    public int delay = 0; // Delay before object becomes active (if used by MainController)

    public string CornerDisappearTag = "CornerDisappearZone"; // NEW: Tag for the corner area where balls disappear after time
    private float _timeInCornerDisappearZone = 0f; // NEW: Timer for how long the ball has been in the disappearance zone
    private bool _isInCornerDisappearZone = false; // NEW: Flag to indicate if the ball is currently in the zone

    public override void Start()
    {
        base.Start(); // Call Draggable's Start method to initialize _renderer, _originalColor, _body
        _dynamicObject = GetComponent<Rigidbody>(); // Get Rigidbody once for direct access
        if (_renderer == null) // Ensure _renderer from Draggable is set in case base.Start() fails or is overridden
        {
            _renderer = GetComponent<MeshRenderer>();
            if (_renderer != null)
            {
                _originalColor = _renderer.material.color;
            }
        }
    }

    // Add a reset method for consistency with RobotController,
    // useful if you want to reset ball positions on simulation reset.
    public void reset()
    {
        if (_dynamicObject != null)
        {
            _dynamicObject.velocity = Vector3.zero;
            _dynamicObject.angularVelocity = Vector3.zero;
            // You could also store and reset to an initial spawn position here.
            // Example: _dynamicObject.transform.position = initialPositionForBall;
            gameObject.SetActive(true); // Ensure ball is active on reset if it was destroyed/deactivated
        }
        // Also reset visual state if needed
        if (_renderer != null)
        {
            _renderer.material.color = _originalColor; // Restore original color
            _renderer.enabled = true; // Ensure it's visible
        }
        // NEW: Reset disappearance timer and flag
        _timeInCornerDisappearZone = 0f;
        _isInCornerDisappearZone = false;
    }

    void Update()
    {
        // Apply boundary bouncing and position clamping for physics objects
        if (_dynamicObject != null)
        {
            _dynamicObject.velocity = FlipVelocityIfOver(_dynamicObject, _arenaMaxX, _arenaMaxY, _arenaMaxZ);
            _dynamicObject.transform.position = EnsurePositionIsWithin(_dynamicObject.transform.position, _arenaMaxX, _arenaMaxY, _arenaMaxZ, 0.1f);
        }

        // Handle visual effects (flickering/ghost) if not highlighted
        if (_renderer == null || IsHighlighted)
        {
            return; // Skip visual effects if no renderer or object is highlighted
        }

        var shouldAlterColor = isFlickering || isGhost;
        var probability = isGhost ? 0.025f : 0.8f; // Ghost objects flicker more often (lower probability for opacity to be 1)
        if (shouldAlterColor && Random.Range(0f, 1f) > probability)
        {
            var opacity = isGhost ? 0f : 0.1f; // Ghost is fully transparent, flickering is mostly transparent
            var color = new Color(_originalColor.r, _originalColor.g, _originalColor.b, opacity);
            _renderer.material.color = color;
        }
        else
        {
            _renderer.material.color = _originalColor; // Restore original color (fully opaque)
        }
    }

    // Called when this object's collider enters another trigger collider.
    // This is now primarily for scoring. Disappearance is handled in OnTriggerStay.
    private void OnTriggerEnter(Collider other)
    {
        // Ensure the other object is tagged "goal" before processing
        if (other.CompareTag("goal"))
        {
            var goalController = other.GetComponent<GoalController>();
            if (goalController != null)
            {
                // Get the value of this dynamic object to update the score
                goalController.UpdateScore(this.value);
        

                // Removed Destroy(gameObject) from here. Disappearance is now timed.
            }
        }
    }

    // NEW: Called when this object's collider stays within another trigger collider.
    void OnTriggerStay(Collider other)
    {
        if (other.CompareTag(CornerDisappearTag))
        {
            _isInCornerDisappearZone = true;
            _timeInCornerDisappearZone += Time.deltaTime; // Increment timer
            
            // Check if ball has been in the zone for 3 seconds
            if (_timeInCornerDisappearZone >= 0.3f)
            {
                Debug.Log($"Dynamic Object ({gameObject.name}) disappeared after 3 seconds in '{CornerDisappearTag}' zone ({other.name}).");
                Destroy(gameObject); // Destroy the ball
            }
        }
    }

    // NEW: Called when this object's collider exits another trigger collider.
    void OnTriggerExit(Collider other)
    {
        if (other.CompareTag(CornerDisappearTag))
        {
            _isInCornerDisappearZone = false;
            _timeInCornerDisappearZone = 0f; // Reset timer
            Debug.Log($"Dynamic Object ({gameObject.name}) exited '{CornerDisappearTag}' zone ({other.name}). Timer reset.");
        }
    }


    // Helper methods for boundary bouncing/positioning (Unchanged)
    float FlipIfOver(float targetVelocity, float currentPosition, float lowerLimit, float upperLimit)
    {
        if (currentPosition > upperLimit || currentPosition < lowerLimit)
        {
            return -_bounceMultiplier * targetVelocity; // Reverse and dampen velocity
        }
        return targetVelocity;
    }

    float EnsureNumberIsWithin(float number, float lowerLimit, float upperLimit, float margin)
    {
        if (number > upperLimit || number < lowerLimit)
        {
            // Clamp the number to be just inside the boundary by the margin
            return Mathf.Clamp(number, lowerLimit + margin, upperLimit - margin);
        }
        return number;
    }

    Vector3 FlipVelocityIfOver(Rigidbody body, float xLimit, float yLimit, float zLimit)
    {
        var position = body.transform.position;
        return new Vector3(
            FlipIfOver(body.velocity.x, position.x, -xLimit, xLimit),
            FlipIfOver(body.velocity.y, position.y, 0, yLimit), // Assuming Y-axis is vertical, 0 is floor
            FlipIfOver(body.velocity.z, position.z, -zLimit, zLimit)
        );
    }

    Vector3 EnsurePositionIsWithin(Vector3 position, float xLimit, float yLimit, float zLimit, float margin)
    {
        return new Vector3(
            EnsureNumberIsWithin(position.x, -xLimit, xLimit, margin),
            EnsureNumberIsWithin(position.y, 0, yLimit, margin), // Assuming Y-axis is vertical, 0 is floor
            EnsureNumberIsWithin(position.z, -zLimit, zLimit, margin)
        );
    }
}

