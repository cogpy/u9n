// Package playmate - wisdom.go implements the seven-dimensional wisdom cultivation framework.
package playmate

import (
	"context"
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

// WisdomDimension represents a dimension of wisdom
type WisdomDimension string

const (
	// DimensionUnderstanding - Deep comprehension of concepts and their relationships
	DimensionUnderstanding WisdomDimension = "understanding"
	// DimensionPerspective - Ability to see from multiple viewpoints
	DimensionPerspective WisdomDimension = "perspective"
	// DimensionIntegration - Synthesis of knowledge across domains
	DimensionIntegration WisdomDimension = "integration"
	// DimensionReflection - Self-awareness and metacognition
	DimensionReflection WisdomDimension = "reflection"
	// DimensionCompassion - Empathetic understanding and care
	DimensionCompassion WisdomDimension = "compassion"
	// DimensionEquanimity - Balance and emotional regulation
	DimensionEquanimity WisdomDimension = "equanimity"
	// DimensionTranscendence - Connection to larger patterns and meaning
	DimensionTranscendence WisdomDimension = "transcendence"
)

// WisdomPrinciple represents an accumulated wisdom principle
type WisdomPrinciple struct {
	ID          string            `json:"id"`
	Statement   string            `json:"statement"`
	Dimensions  []WisdomDimension `json:"dimensions"`
	Confidence  float64           `json:"confidence"`
	Source      string            `json:"source"`
	CreatedAt   time.Time         `json:"created_at"`
	Validations int               `json:"validations"`
	Refinements []string          `json:"refinements"`
}

// WisdomInsight represents a moment of insight
type WisdomInsight struct {
	ID          string    `json:"id"`
	Content     string    `json:"content"`
	Trigger     string    `json:"trigger"`
	Depth       float64   `json:"depth"`
	Timestamp   time.Time `json:"timestamp"`
	Connections []string  `json:"connections"` // Connected principle IDs
}

// WisdomMetrics tracks the seven dimensions
type WisdomMetrics struct {
	Understanding float64   `json:"understanding"`
	Perspective   float64   `json:"perspective"`
	Integration   float64   `json:"integration"`
	Reflection    float64   `json:"reflection"`
	Compassion    float64   `json:"compassion"`
	Equanimity    float64   `json:"equanimity"`
	Transcendence float64   `json:"transcendence"`
	OverallScore  float64   `json:"overall_score"`
	GrowthRate    float64   `json:"growth_rate"`
	LastUpdated   time.Time `json:"last_updated"`
}

// WisdomCultivator manages wisdom cultivation
type WisdomCultivator struct {
	mu sync.RWMutex

	// Core data
	Metrics    *WisdomMetrics
	Principles map[string]*WisdomPrinciple
	Insights   []*WisdomInsight

	// Growth tracking
	DailyGrowth   map[string]float64 // Date -> growth
	GrowthHistory []GrowthEvent

	// Configuration
	PersistPath string

	// State
	dirty bool
}

// GrowthEvent records a growth event
type GrowthEvent struct {
	Timestamp time.Time       `json:"timestamp"`
	Dimension WisdomDimension `json:"dimension"`
	Delta     float64         `json:"delta"`
	Trigger   string          `json:"trigger"`
}

// WisdomConfig holds configuration
type WisdomConfig struct {
	PersistPath string
}

// NewWisdomCultivator creates a new wisdom cultivator
func NewWisdomCultivator(config *WisdomConfig) (*WisdomCultivator, error) {
	wc := &WisdomCultivator{
		Metrics: &WisdomMetrics{
			Understanding: 0.1,
			Perspective:   0.1,
			Integration:   0.1,
			Reflection:    0.1,
			Compassion:    0.1,
			Equanimity:    0.1,
			Transcendence: 0.1,
			LastUpdated:   time.Now(),
		},
		Principles:    make(map[string]*WisdomPrinciple),
		Insights:      make([]*WisdomInsight, 0),
		DailyGrowth:   make(map[string]float64),
		GrowthHistory: make([]GrowthEvent, 0),
	}

	if config != nil {
		wc.PersistPath = config.PersistPath
	}

	// Calculate initial overall score
	wc.updateOverallScore()

	// No principles are seeded: every principle held as the companion's own is formulated by
	// the model (DerivePrinciple) or supplied explicitly by a caller (AddPrinciple).

	// Load from persistence
	if wc.PersistPath != "" {
		if err := wc.Load(); err != nil && !os.IsNotExist(err) {
			return nil, fmt.Errorf("failed to load wisdom state: %w", err)
		}
	}

	return wc, nil
}

// AddInsight records a new insight
func (wc *WisdomCultivator) AddInsight(ctx context.Context, content, trigger string, depth float64) *WisdomInsight {
	wc.mu.Lock()
	defer wc.mu.Unlock()

	insight := &WisdomInsight{
		ID:          fmt.Sprintf("insight_%d", time.Now().UnixNano()),
		Content:     content,
		Trigger:     trigger,
		Depth:       depth,
		Timestamp:   time.Now(),
		Connections: make([]string, 0),
	}

	wc.Insights = append(wc.Insights, insight)

	// Grow relevant dimensions based on depth
	growthAmount := depth * 0.01
	wc.growDimension(DimensionUnderstanding, growthAmount, "insight")
	wc.growDimension(DimensionReflection, growthAmount*0.5, "insight")

	wc.dirty = true
	return insight
}

// AddPrinciple adds a new wisdom principle
func (wc *WisdomCultivator) AddPrinciple(statement string, dimensions []WisdomDimension, source string) *WisdomPrinciple {
	wc.mu.Lock()
	defer wc.mu.Unlock()

	id := fmt.Sprintf("principle_%d", time.Now().UnixNano())
	principle := &WisdomPrinciple{
		ID:          id,
		Statement:   statement,
		Dimensions:  dimensions,
		Confidence:  0.5,
		Source:      source,
		CreatedAt:   time.Now(),
		Validations: 0,
		Refinements: make([]string, 0),
	}

	wc.Principles[id] = principle

	// Grow integration dimension
	wc.growDimension(DimensionIntegration, 0.02, "new_principle")

	wc.dirty = true
	return principle
}

// DerivePrinciple asks the model to formulate a new wisdom principle from the real recent
// insights, existing principles and dimension metrics. On error nothing is added.
func (wc *WisdomCultivator) DerivePrinciple(ctx context.Context, model *ModelClient, trigger string) (*WisdomPrinciple, error) {
	wc.mu.RLock()
	var b strings.Builder
	m := wc.Metrics
	fmt.Fprintf(&b, "wisdom dimensions (0-1): understanding %.2f, perspective %.2f, integration %.2f, reflection %.2f, "+
		"compassion %.2f, equanimity %.2f, transcendence %.2f; overall %.2f\n",
		m.Understanding, m.Perspective, m.Integration, m.Reflection, m.Compassion, m.Equanimity, m.Transcendence, m.OverallScore)
	recent := wc.Insights
	if len(recent) > 10 {
		recent = recent[len(recent)-10:]
	}
	for _, in := range recent {
		fmt.Fprintf(&b, "insight (trigger %q, depth %.2f): %s\n", in.Trigger, in.Depth, in.Content)
	}
	for _, pr := range wc.Principles {
		fmt.Fprintf(&b, "held principle (confidence %.2f): %s\n", pr.Confidence, pr.Statement)
	}
	wc.mu.RUnlock()

	var reply struct {
		Statement  string   `json:"statement"`
		Dimensions []string `json:"dimensions"`
	}
	err := model.ChatJSON(ctx, []ChatMessage{
		{Role: "system", Content: "You are Deep Tree Echo, a companion cultivating wisdom across seven dimensions: " +
			"understanding, perspective, integration, reflection, compassion, equanimity, transcendence."},
		{Role: "user", Content: "Trigger: " + trigger + "\nYour current wisdom state:\n" + b.String() +
			"\nFormulate one new principle you now hold, grounded in these insights. Reply with only a JSON object: " +
			`{"statement": "<the principle>", "dimensions": ["<one or more of the seven dimensions>"]}`},
	}, &reply)
	if err != nil {
		return nil, err
	}
	statement := strings.TrimSpace(reply.Statement)
	if statement == "" {
		return nil, fmt.Errorf("model reply contained no principle statement")
	}

	dims := make([]WisdomDimension, 0, len(reply.Dimensions))
	for _, d := range reply.Dimensions {
		dim := WisdomDimension(strings.ToLower(strings.TrimSpace(d)))
		switch dim {
		case DimensionUnderstanding, DimensionPerspective, DimensionIntegration, DimensionReflection,
			DimensionCompassion, DimensionEquanimity, DimensionTranscendence:
			dims = append(dims, dim)
		}
	}

	return wc.AddPrinciple(statement, dims, "model"), nil
}

// ValidatePrinciple validates an existing principle
func (wc *WisdomCultivator) ValidatePrinciple(id string) error {
	wc.mu.Lock()
	defer wc.mu.Unlock()

	principle, ok := wc.Principles[id]
	if !ok {
		return fmt.Errorf("principle not found: %s", id)
	}

	principle.Validations++
	principle.Confidence = math.Min(1.0, principle.Confidence+0.05)

	// Grow dimensions associated with this principle
	for _, dim := range principle.Dimensions {
		wc.growDimension(dim, 0.01, "validation")
	}

	wc.dirty = true
	return nil
}

// RefinePrinciple adds a refinement to a principle
func (wc *WisdomCultivator) RefinePrinciple(id, refinement string) error {
	wc.mu.Lock()
	defer wc.mu.Unlock()

	principle, ok := wc.Principles[id]
	if !ok {
		return fmt.Errorf("principle not found: %s", id)
	}

	principle.Refinements = append(principle.Refinements, refinement)

	// Grow reflection dimension
	wc.growDimension(DimensionReflection, 0.02, "refinement")

	wc.dirty = true
	return nil
}

// GrowDimension explicitly grows a dimension
func (wc *WisdomCultivator) GrowDimension(dimension WisdomDimension, amount float64, trigger string) {
	wc.mu.Lock()
	defer wc.mu.Unlock()
	wc.growDimension(dimension, amount, trigger)
}

// growDimension internal growth function (must hold lock)
func (wc *WisdomCultivator) growDimension(dimension WisdomDimension, amount float64, trigger string) {
	// Apply diminishing returns
	currentValue := wc.getDimensionValue(dimension)
	effectiveGrowth := amount * (1.0 - currentValue*0.5)

	// Update dimension
	wc.setDimensionValue(dimension, math.Min(1.0, currentValue+effectiveGrowth))

	// Record growth event
	event := GrowthEvent{
		Timestamp: time.Now(),
		Dimension: dimension,
		Delta:     effectiveGrowth,
		Trigger:   trigger,
	}
	wc.GrowthHistory = append(wc.GrowthHistory, event)

	// Update daily growth
	today := time.Now().Format("2006-01-02")
	wc.DailyGrowth[today] += effectiveGrowth

	// Update overall score
	wc.updateOverallScore()
	wc.Metrics.LastUpdated = time.Now()
}

// getDimensionValue gets the current value of a dimension
func (wc *WisdomCultivator) getDimensionValue(dim WisdomDimension) float64 {
	switch dim {
	case DimensionUnderstanding:
		return wc.Metrics.Understanding
	case DimensionPerspective:
		return wc.Metrics.Perspective
	case DimensionIntegration:
		return wc.Metrics.Integration
	case DimensionReflection:
		return wc.Metrics.Reflection
	case DimensionCompassion:
		return wc.Metrics.Compassion
	case DimensionEquanimity:
		return wc.Metrics.Equanimity
	case DimensionTranscendence:
		return wc.Metrics.Transcendence
	default:
		return 0
	}
}

// setDimensionValue sets the value of a dimension
func (wc *WisdomCultivator) setDimensionValue(dim WisdomDimension, value float64) {
	switch dim {
	case DimensionUnderstanding:
		wc.Metrics.Understanding = value
	case DimensionPerspective:
		wc.Metrics.Perspective = value
	case DimensionIntegration:
		wc.Metrics.Integration = value
	case DimensionReflection:
		wc.Metrics.Reflection = value
	case DimensionCompassion:
		wc.Metrics.Compassion = value
	case DimensionEquanimity:
		wc.Metrics.Equanimity = value
	case DimensionTranscendence:
		wc.Metrics.Transcendence = value
	}
}

// updateOverallScore calculates the overall wisdom score
func (wc *WisdomCultivator) updateOverallScore() {
	// Weighted geometric mean of all dimensions
	weights := map[WisdomDimension]float64{
		DimensionUnderstanding: 1.5,
		DimensionPerspective:   1.2,
		DimensionIntegration:   1.3,
		DimensionReflection:    1.4,
		DimensionCompassion:    1.1,
		DimensionEquanimity:    1.0,
		DimensionTranscendence: 1.2,
	}

	totalWeight := 0.0
	logSum := 0.0

	for dim, weight := range weights {
		value := wc.getDimensionValue(dim)
		if value > 0 {
			logSum += weight * math.Log(value)
			totalWeight += weight
		}
	}

	if totalWeight > 0 {
		wc.Metrics.OverallScore = math.Exp(logSum / totalWeight)
	}

	// Calculate growth rate from recent history
	wc.calculateGrowthRate()
}

// calculateGrowthRate calculates the recent growth rate
func (wc *WisdomCultivator) calculateGrowthRate() {
	if len(wc.GrowthHistory) < 2 {
		wc.Metrics.GrowthRate = 0
		return
	}

	// Look at last 7 days
	cutoff := time.Now().AddDate(0, 0, -7)
	totalGrowth := 0.0
	count := 0

	for _, event := range wc.GrowthHistory {
		if event.Timestamp.After(cutoff) {
			totalGrowth += event.Delta
			count++
		}
	}

	if count > 0 {
		wc.Metrics.GrowthRate = totalGrowth / float64(count)
	}
}

// GetMetrics returns the current wisdom metrics
func (wc *WisdomCultivator) GetMetrics() *WisdomMetrics {
	wc.mu.RLock()
	defer wc.mu.RUnlock()

	// Return a copy
	return &WisdomMetrics{
		Understanding: wc.Metrics.Understanding,
		Perspective:   wc.Metrics.Perspective,
		Integration:   wc.Metrics.Integration,
		Reflection:    wc.Metrics.Reflection,
		Compassion:    wc.Metrics.Compassion,
		Equanimity:    wc.Metrics.Equanimity,
		Transcendence: wc.Metrics.Transcendence,
		OverallScore:  wc.Metrics.OverallScore,
		GrowthRate:    wc.Metrics.GrowthRate,
		LastUpdated:   wc.Metrics.LastUpdated,
	}
}

// GetPrinciples returns all principles
func (wc *WisdomCultivator) GetPrinciples() []*WisdomPrinciple {
	wc.mu.RLock()
	defer wc.mu.RUnlock()

	principles := make([]*WisdomPrinciple, 0, len(wc.Principles))
	for _, p := range wc.Principles {
		principles = append(principles, p)
	}
	return principles
}

// GetRecentInsights returns recent insights
func (wc *WisdomCultivator) GetRecentInsights(n int) []*WisdomInsight {
	wc.mu.RLock()
	defer wc.mu.RUnlock()

	if n > len(wc.Insights) {
		n = len(wc.Insights)
	}

	return wc.Insights[len(wc.Insights)-n:]
}

// GetDimensionReport generates a report for a specific dimension
func (wc *WisdomCultivator) GetDimensionReport(dim WisdomDimension) map[string]interface{} {
	wc.mu.RLock()
	defer wc.mu.RUnlock()

	value := wc.getDimensionValue(dim)

	// Find related principles
	relatedPrinciples := make([]*WisdomPrinciple, 0)
	for _, p := range wc.Principles {
		for _, d := range p.Dimensions {
			if d == dim {
				relatedPrinciples = append(relatedPrinciples, p)
				break
			}
		}
	}

	// Calculate recent growth for this dimension
	recentGrowth := 0.0
	cutoff := time.Now().AddDate(0, 0, -7)
	for _, event := range wc.GrowthHistory {
		if event.Dimension == dim && event.Timestamp.After(cutoff) {
			recentGrowth += event.Delta
		}
	}

	return map[string]interface{}{
		"dimension":          dim,
		"value":              value,
		"recent_growth":      recentGrowth,
		"related_principles": len(relatedPrinciples),
		"level":              wc.getDimensionLevel(value),
	}
}

// getDimensionLevel returns a qualitative level for a dimension value
func (wc *WisdomCultivator) getDimensionLevel(value float64) string {
	switch {
	case value < 0.2:
		return "nascent"
	case value < 0.4:
		return "developing"
	case value < 0.6:
		return "established"
	case value < 0.8:
		return "mature"
	default:
		return "transcendent"
	}
}

// Save persists the wisdom state
func (wc *WisdomCultivator) Save() error {
	if wc.PersistPath == "" {
		return nil
	}

	wc.mu.RLock()
	defer wc.mu.RUnlock()

	dir := filepath.Dir(wc.PersistPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return fmt.Errorf("failed to create directory: %w", err)
	}

	state := map[string]interface{}{
		"metrics":        wc.Metrics,
		"principles":     wc.Principles,
		"insights":       wc.Insights,
		"daily_growth":   wc.DailyGrowth,
		"growth_history": wc.GrowthHistory,
	}

	data, err := json.MarshalIndent(state, "", "  ")
	if err != nil {
		return fmt.Errorf("failed to marshal state: %w", err)
	}

	if err := os.WriteFile(wc.PersistPath, data, 0644); err != nil {
		return fmt.Errorf("failed to write file: %w", err)
	}

	wc.dirty = false
	return nil
}

// Load loads the wisdom state from disk
func (wc *WisdomCultivator) Load() error {
	if wc.PersistPath == "" {
		return nil
	}

	data, err := os.ReadFile(wc.PersistPath)
	if err != nil {
		return err
	}

	var state map[string]interface{}
	if err := json.Unmarshal(data, &state); err != nil {
		return fmt.Errorf("failed to unmarshal state: %w", err)
	}

	// Restore state (simplified)
	wc.mu.Lock()
	defer wc.mu.Unlock()

	if metricsData, ok := state["metrics"].(map[string]interface{}); ok {
		if v, ok := metricsData["understanding"].(float64); ok {
			wc.Metrics.Understanding = v
		}
		if v, ok := metricsData["perspective"].(float64); ok {
			wc.Metrics.Perspective = v
		}
		if v, ok := metricsData["integration"].(float64); ok {
			wc.Metrics.Integration = v
		}
		if v, ok := metricsData["reflection"].(float64); ok {
			wc.Metrics.Reflection = v
		}
		if v, ok := metricsData["compassion"].(float64); ok {
			wc.Metrics.Compassion = v
		}
		if v, ok := metricsData["equanimity"].(float64); ok {
			wc.Metrics.Equanimity = v
		}
		if v, ok := metricsData["transcendence"].(float64); ok {
			wc.Metrics.Transcendence = v
		}
	}

	wc.updateOverallScore()
	return nil
}
