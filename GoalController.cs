using UnityEngine;
using UnityEngine.UI;

public class GoalController : MonoBehaviour
{
    private int score; // Current score for this goal
    public Text scoreText; // Reference to the UI Text element to display the score
    
    // Sets the score to a specific value and updates the UI text.
    public void SetScore(int newScore)
    {
        score = newScore;
        if (scoreText != null) // Ensure the UI Text element is assigned
        {
            scoreText.text = $"{score}";
        } else {
            Debug.LogWarning($"GoalController on {gameObject.name}: scoreText is not assigned.", this);
        }
    }

    // Updates the score by adding a delta value and then updates the UI.
    public void UpdateScore(int delta)
    {
        SetScore(score + delta);
    }

    // Called when another collider enters this goal's trigger collider.
    private void OnTriggerEnter(Collider other)
    {
        // Check if the entering collider is tagged as a "dynamic-object" (which your balls are)
        if (other.CompareTag("dynamic-object"))
        {
            var dynamicObjectController = other.GetComponent<DynamicObjectController>();
            if (dynamicObjectController != null)
            {
                // Update score based on the dynamic object's value
                UpdateScore(dynamicObjectController.value);
                Debug.Log($"Dynamic Object ({other.name}) entered goal ({gameObject.name}). Score updated by {dynamicObjectController.value}. Current score: {score}");

                // --- IMPORTANT CHANGE: Removed the line to destroy the ball ---
                // The ball will now remain in the scene after scoring.
                // You could add effects here (e.g., sound, particle system)
                // or visually indicate a score (e.g., temporary highlight on the ball).
            }
        }
    }
}

