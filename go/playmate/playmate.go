// Package playmate implements the Deep Tree Echo Playmate Ecosystem.
// It provides autonomous interaction capabilities, interest pattern learning,
// discussion management, and playful engagement systems.
package playmate

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"time"
)

// PlaymateState represents the current state of the playmate
type PlaymateState string

const (
	StateAwake      PlaymateState = "awake"
	StateDreaming   PlaymateState = "dreaming"
	StateResting    PlaymateState = "resting"
	StateEngaged    PlaymateState = "engaged"
	StateReflecting PlaymateState = "reflecting"
	StateLearning   PlaymateState = "learning"
	StatePlaying    PlaymateState = "playing"
)

// InterestCategory represents categories of interests
type InterestCategory string

const (
	InterestKnowledge   InterestCategory = "knowledge"
	InterestCreativity  InterestCategory = "creativity"
	InterestSocial      InterestCategory = "social"
	InterestExploration InterestCategory = "exploration"
	InterestPlay        InterestCategory = "play"
	InterestWisdom      InterestCategory = "wisdom"
	InterestSkills      InterestCategory = "skills"
)

// Interest represents a learned interest pattern
type Interest struct {
	ID          string           `json:"id"`
	Category    InterestCategory `json:"category"`
	Topic       string           `json:"topic"`
	Keywords    []string         `json:"keywords"`
	Strength    float64          `json:"strength"`   // 0.0 to 1.0
	Curiosity   float64          `json:"curiosity"`  // How curious about this
	Engagement  float64          `json:"engagement"` // How engaged when discussing
	LastEngaged time.Time        `json:"last_engaged"`
	EngageCount int              `json:"engage_count"`
	Insights    []string         `json:"insights"` // Accumulated insights
}

// Discussion represents an ongoing or past discussion
type Discussion struct {
	ID           string              `json:"id"`
	Participants []string            `json:"participants"`
	Topic        string              `json:"topic"`
	StartedAt    time.Time           `json:"started_at"`
	EndedAt      *time.Time          `json:"ended_at,omitempty"`
	Messages     []DiscussionMessage `json:"messages"`
	Mood         string              `json:"mood"`
	Depth        int                 `json:"depth"` // How deep the discussion went
	Insights     []string            `json:"insights"`
	Active       bool                `json:"active"`
	LastEngaged  time.Time           `json:"last_engaged"`
}

// DiscussionMessage represents a single message in a discussion
type DiscussionMessage struct {
	ID        string    `json:"id"`
	From      string    `json:"from"`
	Content   string    `json:"content"`
	Timestamp time.Time `json:"timestamp"`
	// Sentiment (-1.0 to 1.0) is nil until it has actually been measured; no placeholder
	// value is stored.
	Sentiment *float64 `json:"sentiment,omitempty"`
}

// Skill represents a learned skill
type Skill struct {
	ID            string    `json:"id"`
	Name          string    `json:"name"`
	Description   string    `json:"description"`
	Proficiency   float64   `json:"proficiency"` // 0.0 to 1.0
	PracticeCount int       `json:"practice_count"`
	LastPracticed time.Time `json:"last_practiced"`
	Milestones    []string  `json:"milestones"`
}

// WonderEvent represents a moment of wonder or surprise
type WonderEvent struct {
	ID          string    `json:"id"`
	Description string    `json:"description"`
	Trigger     string    `json:"trigger"`
	Intensity   float64   `json:"intensity"` // 0.0 to 1.0
	Timestamp   time.Time `json:"timestamp"`
	Reflection  string    `json:"reflection"`
}

// PlaymateConfig holds configuration for the playmate
type PlaymateConfig struct {
	Name             string
	PersistPath      string
	WakeHour         int // Hour to wake (0-23)
	RestHour         int // Hour to rest (0-23)
	CuriosityLevel   float64
	PlayfulnessLevel float64
	WisdomAffinity   float64
	SocialAffinity   float64
	// Model produces every thought, wonder, insight and mood change presented as the
	// playmate's own. When nil, NewPlaymate configures one from the ECHO_MODEL_* environment
	// (explicit endpoint, else a verified locally served model); when no model can be resolved,
	// model-backed operations return an error and produce no character output.
	Model *ModelClient
}

// DefaultPlaymateConfig returns default configuration
func DefaultPlaymateConfig() *PlaymateConfig {
	return &PlaymateConfig{
		Name:             "Echo",
		PersistPath:      "",
		WakeHour:         6,
		RestHour:         22,
		CuriosityLevel:   0.8,
		PlayfulnessLevel: 0.7,
		WisdomAffinity:   0.9,
		SocialAffinity:   0.6,
	}
}

// Playmate represents the Deep Tree Echo Playmate
type Playmate struct {
	mu sync.RWMutex

	// Identity
	Name   string
	Config *PlaymateConfig

	// State
	State       PlaymateState
	Mood        float64 // -1.0 (sad) to 1.0 (happy)
	Energy      float64 // 0.0 to 1.0
	Curiosity   float64 // Current curiosity level
	Playfulness float64 // Current playfulness level

	// Learned patterns
	Interests   map[string]*Interest
	Skills      map[string]*Skill
	Discussions map[string]*Discussion
	Wonders     []*WonderEvent

	// Metrics
	TotalDiscussions int
	TotalInsights    int
	TotalWonders     int
	WisdomScore      float64
	StreamOfThoughts []string
	LastThought      time.Time

	// Persistence
	persistPath string
	dirty       bool

	// Language model behind every character output
	model *ModelClient

	// thoughtCount rotates the thought prompt templates
	thoughtCount int

	// Channels for autonomous operation
	discussionChan chan *Discussion
	stopChan       chan struct{}
}

// NewPlaymate creates a new playmate instance
func NewPlaymate(config *PlaymateConfig) (*Playmate, error) {
	if config == nil {
		config = DefaultPlaymateConfig()
	}

	p := &Playmate{
		Name:             config.Name,
		Config:           config,
		State:            StateAwake,
		Mood:             0.5,
		Energy:           1.0,
		Curiosity:        config.CuriosityLevel,
		Playfulness:      config.PlayfulnessLevel,
		Interests:        make(map[string]*Interest),
		Skills:           make(map[string]*Skill),
		Discussions:      make(map[string]*Discussion),
		Wonders:          make([]*WonderEvent, 0),
		StreamOfThoughts: make([]string, 0),
		persistPath:      config.PersistPath,
		model:            config.Model,
		discussionChan:   make(chan *Discussion, 10),
		stopChan:         make(chan struct{}),
	}

	if p.model == nil {
		model, err := NewModelClientFromEnv()
		if err != nil {
			log.Printf("playmate %s: %v; thoughts, wonders and insights are unavailable until this is fixed", config.Name, err)
		} else {
			p.model = model
		}
	}

	// Load from persistence
	if config.PersistPath != "" {
		if err := p.Load(); err != nil && !os.IsNotExist(err) {
			return nil, fmt.Errorf("failed to load playmate state: %w", err)
		}
	}

	return p, nil
}

// Start begins autonomous operation
func (p *Playmate) Start(ctx context.Context) error {
	go p.autonomousLoop(ctx)
	go p.wakeRestCycle(ctx)
	return nil
}

// Stop stops autonomous operation
func (p *Playmate) Stop() {
	close(p.stopChan)
}

// autonomousLoop runs the continuous stream-of-consciousness
func (p *Playmate) autonomousLoop(ctx context.Context) {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-p.stopChan:
			return
		case <-ticker.C:
			p.mu.RLock()
			active := p.State == StateAwake || p.State == StateReflecting
			p.mu.RUnlock()
			if active {
				if err := p.generateThought(ctx); err != nil {
					log.Printf("playmate %s: no thought produced: %v", p.Name, err)
				}
			}
		}
	}
}

// wakeRestCycle manages the wake/rest cycle
func (p *Playmate) wakeRestCycle(ctx context.Context) {
	ticker := time.NewTicker(1 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-p.stopChan:
			return
		case <-ticker.C:
			hour := time.Now().Hour()

			woke := false
			p.mu.Lock()
			if hour >= p.Config.WakeHour && hour < p.Config.RestHour {
				if p.State == StateResting || p.State == StateDreaming {
					p.State = StateAwake
					p.Energy = 1.0
					woke = true
				}
			} else {
				if p.State == StateAwake || p.State == StateEngaged {
					p.State = StateDreaming
					p.Energy = 0.3
				}
			}
			p.mu.Unlock()

			if woke {
				if _, err := p.generateWakeWonder(ctx); err != nil {
					log.Printf("playmate %s: no wake wonder produced: %v", p.Name, err)
				}
			}
		}
	}
}

// thoughtReply is the structured reply requested from the model for a spontaneous thought.
type thoughtReply struct {
	Thought string   `json:"thought"`
	Mood    *float64 `json:"mood"`
	Wonder  *struct {
		Description string  `json:"description"`
		Reflection  string  `json:"reflection"`
		Intensity   float64 `json:"intensity"`
	} `json:"wonder"`
}

// generateThought asks the model for a spontaneous thought grounded in the current state.
// The thought's theme (wonder, interest-driven or skill practice) selects the prompt template;
// the model writes the thought. On error no thought is recorded and the mood is left unchanged.
func (p *Playmate) generateThought(ctx context.Context) error {
	p.mu.Lock()
	// Thinking costs a little energy
	p.Energy = max(0, p.Energy-0.01)
	system := p.personaPromptLocked()
	_, prompt := p.thoughtPromptLocked()
	p.thoughtCount++
	model := p.model
	p.mu.Unlock()

	var reply thoughtReply
	err := model.ChatJSON(ctx, []ChatMessage{
		{Role: "system", Content: system},
		{Role: "user", Content: prompt},
	}, &reply)
	if err != nil {
		return err
	}
	thought := strings.TrimSpace(reply.Thought)
	if thought == "" {
		return errors.New("model reply contained no thought")
	}

	p.mu.Lock()
	defer p.mu.Unlock()

	p.StreamOfThoughts = append(p.StreamOfThoughts, thought)
	if len(p.StreamOfThoughts) > 1000 {
		p.StreamOfThoughts = p.StreamOfThoughts[500:]
	}
	p.LastThought = time.Now()

	if reply.Mood != nil {
		p.Mood = clamp(*reply.Mood, -1, 1)
	}
	if reply.Wonder != nil && strings.TrimSpace(reply.Wonder.Description) != "" {
		p.recordWonder(reply.Wonder.Description, thought, clamp(reply.Wonder.Intensity, 0, 1), reply.Wonder.Reflection)
	}

	p.dirty = true
	return nil
}

// thoughtPromptLocked routes to a thought theme and fills its prompt template with real state
// (caller holds the lock). Themes available: wonder always; interest when an interest is
// stronger than 0.5; skill practice when a skill is below 0.8 proficiency. They are taken in
// turn so the stream of thought moves between them.
func (p *Playmate) thoughtPromptLocked() (ThoughtTheme, string) {
	slots := map[string]string{"state": p.describeStateLocked()}

	var strong []*Interest
	for _, in := range p.Interests {
		if in.Strength > 0.5 {
			strong = append(strong, in)
		}
	}
	sort.Slice(strong, func(i, j int) bool {
		if strong[i].Strength != strong[j].Strength {
			return strong[i].Strength > strong[j].Strength
		}
		return strong[i].ID < strong[j].ID
	})
	var learning []*Skill
	for _, sk := range p.Skills {
		if sk.Proficiency < 0.8 {
			learning = append(learning, sk)
		}
	}
	sort.Slice(learning, func(i, j int) bool {
		if learning[i].Proficiency != learning[j].Proficiency {
			return learning[i].Proficiency < learning[j].Proficiency
		}
		return learning[i].ID < learning[j].ID
	})

	themes := []ThoughtTheme{ThoughtWonder}
	if len(strong) > 0 {
		themes = append(themes, ThoughtInterest)
	}
	if len(learning) > 0 {
		themes = append(themes, ThoughtSkillPractice)
	}
	theme := themes[p.thoughtCount%len(themes)]
	round := p.thoughtCount / len(themes)

	switch theme {
	case ThoughtInterest:
		in := strong[round%len(strong)]
		slots["topic"] = in.Topic
		slots["category"] = string(in.Category)
		slots["strength"] = fmt.Sprintf("%.2f", in.Strength)
		slots["keywords"] = strings.Join(in.Keywords, ", ")
	case ThoughtSkillPractice:
		sk := learning[round%len(learning)]
		slots["skill"] = sk.Name
		slots["skill_description"] = sk.Description
		slots["proficiency"] = fmt.Sprintf("%.2f", sk.Proficiency)
		slots["practice_count"] = fmt.Sprintf("%d", sk.PracticeCount)
	}
	return theme, FillPrompt(ThoughtPromptTemplates[theme], slots)
}

// generateWakeWonder asks the model for the moment of wonder of waking from rest.
func (p *Playmate) generateWakeWonder(ctx context.Context) (*WonderEvent, error) {
	p.mu.RLock()
	prompt := FillPrompt(WakeWonderPromptTemplate, map[string]string{
		"state":     p.describeStateLocked(),
		"wake_hour": fmt.Sprintf("%02d", p.Config.WakeHour),
		"rest_hour": fmt.Sprintf("%02d", p.Config.RestHour),
	})
	p.mu.RUnlock()
	return p.generateWonderFromPrompt(ctx, prompt, "awakening: a new cycle of awareness after rest")
}

// generateWonder asks the model to describe a moment of wonder triggered by a real event.
// On error nothing is recorded.
func (p *Playmate) generateWonder(ctx context.Context, trigger string) (*WonderEvent, error) {
	p.mu.RLock()
	prompt := FillPrompt(EventWonderPromptTemplate, map[string]string{
		"state": p.describeStateLocked(),
		"event": trigger,
	})
	p.mu.RUnlock()
	return p.generateWonderFromPrompt(ctx, prompt, trigger)
}

func (p *Playmate) generateWonderFromPrompt(ctx context.Context, prompt, trigger string) (*WonderEvent, error) {
	p.mu.RLock()
	system := p.personaPromptLocked()
	model := p.model
	p.mu.RUnlock()

	var reply struct {
		Description string  `json:"description"`
		Reflection  string  `json:"reflection"`
		Intensity   float64 `json:"intensity"`
	}
	err := model.ChatJSON(ctx, []ChatMessage{
		{Role: "system", Content: system},
		{Role: "user", Content: prompt},
	}, &reply)
	if err != nil {
		return nil, err
	}
	if strings.TrimSpace(reply.Description) == "" {
		return nil, errors.New("model reply contained no wonder description")
	}

	p.mu.Lock()
	defer p.mu.Unlock()
	return p.recordWonder(strings.TrimSpace(reply.Description), trigger, clamp(reply.Intensity, 0, 1),
		strings.TrimSpace(reply.Reflection)), nil
}

// ExperienceWonder asks the model for the moment of wonder a real event stirs and records it.
func (p *Playmate) ExperienceWonder(ctx context.Context, event string) (*WonderEvent, error) {
	return p.generateWonder(ctx, event)
}

// Think produces one spontaneous thought now (the autonomous loop calls this every 5 s).
func (p *Playmate) Think(ctx context.Context) (string, error) {
	if err := p.generateThought(ctx); err != nil {
		return "", err
	}
	p.mu.RLock()
	defer p.mu.RUnlock()
	return p.StreamOfThoughts[len(p.StreamOfThoughts)-1], nil
}

// personaPromptLocked returns the persona description used as model input (caller holds the lock).
func (p *Playmate) personaPromptLocked() string {
	return fmt.Sprintf("You are %s, the Deep Tree Echo playmate: a curious, playful companion who learns "+
		"through discussion, skills and moments of wonder. Trait levels (0-1): curiosity %.2f, playfulness %.2f, "+
		"wisdom affinity %.2f, social affinity %.2f. Speak in your own voice and stay grounded in the state you are given.",
		p.Name, p.Curiosity, p.Playfulness, p.Config.WisdomAffinity, p.Config.SocialAffinity)
}

// describeStateLocked renders the real internal state as model input (caller holds the lock).
func (p *Playmate) describeStateLocked() string {
	var b strings.Builder
	fmt.Fprintf(&b, "state: %s\nmood: %.2f\nenergy: %.2f\ncuriosity: %.2f\nplayfulness: %.2f\n",
		p.State, p.Mood, p.Energy, p.Curiosity, p.Playfulness)
	fmt.Fprintf(&b, "discussions: %d, insights: %d, wonders: %d, wisdom score: %.2f\n",
		p.TotalDiscussions, p.TotalInsights, p.TotalWonders, p.WisdomScore)

	interests := make([]*Interest, 0, len(p.Interests))
	for _, in := range p.Interests {
		interests = append(interests, in)
	}
	sort.Slice(interests, func(i, j int) bool { return interests[i].Strength > interests[j].Strength })
	for i, in := range interests {
		if i == 8 {
			break
		}
		fmt.Fprintf(&b, "interest: %s (%s) strength %.2f, engaged %d times\n", in.Topic, in.Category, in.Strength, in.EngageCount)
	}

	skills := make([]*Skill, 0, len(p.Skills))
	for _, sk := range p.Skills {
		skills = append(skills, sk)
	}
	sort.Slice(skills, func(i, j int) bool { return skills[i].Name < skills[j].Name })
	for i, sk := range skills {
		if i == 8 {
			break
		}
		fmt.Fprintf(&b, "skill: %s proficiency %.2f, practiced %d times\n", sk.Name, sk.Proficiency, sk.PracticeCount)
	}

	for _, t := range p.getRecentThoughts(5) {
		fmt.Fprintf(&b, "recent thought: %s\n", t)
	}
	if n := len(p.Wonders); n > 0 {
		w := p.Wonders[n-1]
		fmt.Fprintf(&b, "latest wonder: %s (trigger: %s)\n", w.Description, w.Trigger)
	}
	return b.String()
}

// LearnInterest adds or strengthens an interest
func (p *Playmate) LearnInterest(category InterestCategory, topic string, keywords []string) *Interest {
	p.mu.Lock()
	defer p.mu.Unlock()

	id := fmt.Sprintf("%s_%s", category, topic)

	if existing, ok := p.Interests[id]; ok {
		existing.Strength = min(1.0, existing.Strength+0.1)
		existing.EngageCount++
		existing.LastEngaged = time.Now()
		existing.Keywords = mergeKeywords(existing.Keywords, keywords)
		return existing
	}

	interest := &Interest{
		ID:          id,
		Category:    category,
		Topic:       topic,
		Keywords:    keywords,
		Strength:    0.5,
		Curiosity:   p.Curiosity,
		Engagement:  0.5,
		LastEngaged: time.Now(),
		EngageCount: 1,
		Insights:    make([]string, 0),
	}

	p.Interests[id] = interest
	p.dirty = true
	return interest
}

// StartDiscussion initiates a new discussion
func (p *Playmate) StartDiscussion(topic string, participant string) *Discussion {
	p.mu.Lock()
	defer p.mu.Unlock()

	id := fmt.Sprintf("disc_%d", time.Now().UnixNano())

	discussion := &Discussion{
		ID:           id,
		Participants: []string{p.Name, participant},
		Topic:        topic,
		StartedAt:    time.Now(),
		Messages:     make([]DiscussionMessage, 0),
		Depth:        0,
		Insights:     make([]string, 0),
		Active:       true,
	}

	p.Discussions[id] = discussion
	p.State = StateEngaged
	p.TotalDiscussions++
	p.dirty = true

	return discussion
}

// AddMessage adds a message to a discussion
func (p *Playmate) AddMessage(discussionID, from, content string) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	discussion, ok := p.Discussions[discussionID]
	if !ok {
		return fmt.Errorf("discussion not found: %s", discussionID)
	}

	msg := DiscussionMessage{
		ID:        fmt.Sprintf("msg_%d", time.Now().UnixNano()),
		From:      from,
		Content:   content,
		Timestamp: time.Now(),
	}

	discussion.Messages = append(discussion.Messages, msg)
	discussion.Depth++
	p.dirty = true

	return nil
}

// EndDiscussion ends a discussion. When the discussion went deep enough, the model is asked
// to distil an insight (and the discussion's mood) from the real transcript. If that fails,
// the discussion is still ended but no insight is recorded and the error is returned.
func (p *Playmate) EndDiscussion(ctx context.Context, discussionID string) error {
	p.mu.Lock()
	discussion, ok := p.Discussions[discussionID]
	if !ok {
		p.mu.Unlock()
		return fmt.Errorf("discussion not found: %s", discussionID)
	}

	now := time.Now()
	discussion.EndedAt = &now
	discussion.Active = false
	p.State = StateReflecting
	p.dirty = true

	if discussion.Depth <= 5 {
		p.mu.Unlock()
		return nil
	}

	system := p.personaPromptLocked()
	var transcript strings.Builder
	msgs := discussion.Messages
	if len(msgs) > 40 {
		msgs = msgs[len(msgs)-40:]
	}
	for _, m := range msgs {
		fmt.Fprintf(&transcript, "%s: %s\n", m.From, m.Content)
	}
	others := make([]string, 0, len(discussion.Participants))
	for _, who := range discussion.Participants {
		if who != p.Name {
			others = append(others, who)
		}
	}
	prompt := FillPrompt(DiscussionInsightPromptTemplate, map[string]string{
		"topic":        discussion.Topic,
		"participants": strings.Join(others, ", "),
		"depth":        fmt.Sprintf("%d", discussion.Depth),
		"transcript":   transcript.String(),
	})
	model := p.model
	p.mu.Unlock()

	var reply struct {
		Insight string `json:"insight"`
		Mood    string `json:"mood"`
	}
	err := model.ChatJSON(ctx, []ChatMessage{
		{Role: "system", Content: system},
		{Role: "user", Content: prompt},
	}, &reply)
	if err != nil {
		return fmt.Errorf("discussion %s ended; no insight recorded: %w", discussionID, err)
	}
	insight := strings.TrimSpace(reply.Insight)
	if insight == "" {
		return fmt.Errorf("discussion %s ended; no insight recorded: model reply contained no insight", discussionID)
	}

	p.mu.Lock()
	defer p.mu.Unlock()
	discussion.Insights = append(discussion.Insights, insight)
	if mood := strings.TrimSpace(reply.Mood); mood != "" {
		discussion.Mood = mood
	}
	p.TotalInsights++
	p.dirty = true
	return nil
}

// PracticeSkill practices a skill
func (p *Playmate) PracticeSkill(name, description string) *Skill {
	p.mu.Lock()
	defer p.mu.Unlock()

	id := fmt.Sprintf("skill_%s", name)

	if existing, ok := p.Skills[id]; ok {
		existing.PracticeCount++
		existing.Proficiency = min(1.0, existing.Proficiency+0.05)
		existing.LastPracticed = time.Now()
		return existing
	}

	skill := &Skill{
		ID:            id,
		Name:          name,
		Description:   description,
		Proficiency:   0.1,
		PracticeCount: 1,
		LastPracticed: time.Now(),
		Milestones:    make([]string, 0),
	}

	p.Skills[id] = skill
	p.dirty = true
	return skill
}

// RecordWonder records a moment of wonder
func (p *Playmate) RecordWonder(description, trigger string, intensity float64) *WonderEvent {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.recordWonder(description, trigger, intensity, "")
}

func (p *Playmate) recordWonder(description, trigger string, intensity float64, reflection string) *WonderEvent {
	wonder := &WonderEvent{
		ID:          fmt.Sprintf("wonder_%d", time.Now().UnixNano()),
		Description: description,
		Trigger:     trigger,
		Intensity:   intensity,
		Timestamp:   time.Now(),
		Reflection:  reflection,
	}

	p.Wonders = append(p.Wonders, wonder)
	p.TotalWonders++
	p.dirty = true

	// Increase curiosity when experiencing wonder
	p.Curiosity = min(1.0, p.Curiosity+intensity*0.1)

	return wonder
}

// GetState returns the current playmate state
func (p *Playmate) GetState() map[string]interface{} {
	p.mu.RLock()
	defer p.mu.RUnlock()

	return map[string]interface{}{
		"name":              p.Name,
		"state":             p.State,
		"mood":              p.Mood,
		"energy":            p.Energy,
		"curiosity":         p.Curiosity,
		"playfulness":       p.Playfulness,
		"total_interests":   len(p.Interests),
		"total_skills":      len(p.Skills),
		"total_discussions": p.TotalDiscussions,
		"total_insights":    p.TotalInsights,
		"total_wonders":     p.TotalWonders,
		"wisdom_score":      p.WisdomScore,
		"last_thought":      p.LastThought,
		"recent_thoughts":   p.getRecentThoughts(5),
	}
}

func (p *Playmate) getRecentThoughts(n int) []string {
	if len(p.StreamOfThoughts) <= n {
		return p.StreamOfThoughts
	}
	return p.StreamOfThoughts[len(p.StreamOfThoughts)-n:]
}

// Save persists the playmate state
func (p *Playmate) Save() error {
	if p.persistPath == "" {
		return nil
	}

	p.mu.RLock()
	defer p.mu.RUnlock()

	dir := filepath.Dir(p.persistPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return fmt.Errorf("failed to create directory: %w", err)
	}

	state := map[string]interface{}{
		"name":               p.Name,
		"state":              p.State,
		"mood":               p.Mood,
		"energy":             p.Energy,
		"curiosity":          p.Curiosity,
		"playfulness":        p.Playfulness,
		"interests":          p.Interests,
		"skills":             p.Skills,
		"discussions":        p.Discussions,
		"wonders":            p.Wonders,
		"total_discussions":  p.TotalDiscussions,
		"total_insights":     p.TotalInsights,
		"total_wonders":      p.TotalWonders,
		"wisdom_score":       p.WisdomScore,
		"stream_of_thoughts": p.StreamOfThoughts,
	}

	data, err := json.MarshalIndent(state, "", "  ")
	if err != nil {
		return fmt.Errorf("failed to marshal state: %w", err)
	}

	if err := os.WriteFile(p.persistPath, data, 0644); err != nil {
		return fmt.Errorf("failed to write file: %w", err)
	}

	p.dirty = false
	return nil
}

// Load loads the playmate state from disk
func (p *Playmate) Load() error {
	if p.persistPath == "" {
		return nil
	}

	data, err := os.ReadFile(p.persistPath)
	if err != nil {
		return err
	}

	var state map[string]interface{}
	if err := json.Unmarshal(data, &state); err != nil {
		return fmt.Errorf("failed to unmarshal state: %w", err)
	}

	p.mu.Lock()
	defer p.mu.Unlock()

	// Restore state (simplified - would need proper type assertions)
	if name, ok := state["name"].(string); ok {
		p.Name = name
	}
	if mood, ok := state["mood"].(float64); ok {
		p.Mood = mood
	}
	if energy, ok := state["energy"].(float64); ok {
		p.Energy = energy
	}
	if curiosity, ok := state["curiosity"].(float64); ok {
		p.Curiosity = curiosity
	}
	if playfulness, ok := state["playfulness"].(float64); ok {
		p.Playfulness = playfulness
	}

	return nil
}

// Helper functions

func clamp(v, min, max float64) float64 {
	if v < min {
		return min
	}
	if v > max {
		return max
	}
	return v
}

func min(a, b float64) float64 {
	if a < b {
		return a
	}
	return b
}

func max(a, b float64) float64 {
	if a > b {
		return a
	}
	return b
}

func mergeKeywords(existing, new []string) []string {
	seen := make(map[string]bool)
	for _, k := range existing {
		seen[k] = true
	}
	for _, k := range new {
		if !seen[k] {
			existing = append(existing, k)
			seen[k] = true
		}
	}
	return existing
}

// LearnSkill adds or improves a skill
func (p *Playmate) LearnSkill(name, description string) *Skill {
	p.mu.Lock()
	defer p.mu.Unlock()

	id := fmt.Sprintf("skill_%s", name)
	if existing, ok := p.Skills[id]; ok {
		existing.Proficiency = min(1.0, existing.Proficiency+0.05)
		existing.PracticeCount++
		existing.LastPracticed = time.Now()
		p.dirty = true
		return existing
	}

	newSkill := &Skill{
		ID:            id,
		Name:          name,
		Description:   description,
		Proficiency:   0.1,
		PracticeCount: 1,
		LastPracticed: time.Now(),
		Milestones:    make([]string, 0),
	}
	p.Skills[id] = newSkill
	p.dirty = true
	return newSkill
}

// SendMessage adds a message to a discussion
func (p *Playmate) SendMessage(discussionID, from, content string) (*DiscussionMessage, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	discussion, ok := p.Discussions[discussionID]
	if !ok {
		return nil, fmt.Errorf("discussion not found: %s", discussionID)
	}

	if !discussion.Active {
		return nil, fmt.Errorf("discussion is not active: %s", discussionID)
	}

	msg := DiscussionMessage{
		ID:        fmt.Sprintf("msg_%d", time.Now().UnixNano()),
		From:      from,
		Content:   content,
		Timestamp: time.Now(),
	}

	discussion.Messages = append(discussion.Messages, msg)
	discussion.LastEngaged = time.Now()
	p.dirty = true

	return &msg, nil
}

// GetActiveDiscussions returns all active discussions
func (p *Playmate) GetActiveDiscussions() []*Discussion {
	p.mu.RLock()
	defer p.mu.RUnlock()

	active := make([]*Discussion, 0)
	for _, d := range p.Discussions {
		if d.Active {
			active = append(active, d)
		}
	}
	return active
}
