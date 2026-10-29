package playmate

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
)

// fakeModelServer is a test-only stand-in for a model server (Ollama /api/tags, OpenAI
// /v1/models and /v1/chat/completions). Replies come from the test's reply function.
type fakeModelServer struct {
	*httptest.Server
	mu      sync.Mutex
	models  []string
	prompts []string // last user message of each chat request
	models_ []string // model field of each chat request
	auth    []string // Authorization header of each chat request
	reply   func(prompt string) string
}

func newFakeModelServer(t *testing.T, models []string, reply func(prompt string) string) *fakeModelServer {
	t.Helper()
	f := &fakeModelServer{models: models, reply: reply}
	mux := http.NewServeMux()
	mux.HandleFunc("/api/tags", func(w http.ResponseWriter, r *http.Request) {
		list := make([]map[string]string, 0, len(f.models))
		for _, m := range f.models {
			list = append(list, map[string]string{"name": m})
		}
		json.NewEncoder(w).Encode(map[string]interface{}{"models": list})
	})
	mux.HandleFunc("/v1/models", func(w http.ResponseWriter, r *http.Request) {
		list := make([]map[string]string, 0, len(f.models))
		for _, m := range f.models {
			list = append(list, map[string]string{"id": m})
		}
		json.NewEncoder(w).Encode(map[string]interface{}{"object": "list", "data": list})
	})
	mux.HandleFunc("/v1/chat/completions", func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			Model    string        `json:"model"`
			Messages []ChatMessage `json:"messages"`
		}
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			http.Error(w, err.Error(), http.StatusBadRequest)
			return
		}
		prompt := ""
		if n := len(req.Messages); n > 0 {
			prompt = req.Messages[n-1].Content
		}
		f.mu.Lock()
		f.prompts = append(f.prompts, prompt)
		f.models_ = append(f.models_, req.Model)
		f.auth = append(f.auth, r.Header.Get("Authorization"))
		f.mu.Unlock()
		json.NewEncoder(w).Encode(map[string]interface{}{
			"choices": []map[string]interface{}{{"message": map[string]string{"role": "assistant", "content": f.reply(prompt)}}},
		})
	})
	f.Server = httptest.NewServer(mux)
	t.Cleanup(f.Close)
	return f
}

func (f *fakeModelServer) chatCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return len(f.prompts)
}

func (f *fakeModelServer) lastPrompt() string {
	f.mu.Lock()
	defer f.mu.Unlock()
	if len(f.prompts) == 0 {
		return ""
	}
	return f.prompts[len(f.prompts)-1]
}

func ollamaProbe(f *fakeModelServer) localProbe {
	return localProbe{listURL: f.URL + "/api/tags", baseURL: f.URL + "/v1", ollamaTags: true}
}

func openAIProbe(f *fakeModelServer) localProbe {
	return localProbe{listURL: f.URL + "/v1/models", baseURL: f.URL + "/v1"}
}

func echoReply(text string) func(string) string { return func(string) string { return text } }

func TestExplicitConfigIsUsed(t *testing.T) {
	api := newFakeModelServer(t, nil, echoReply("hello from the configured model"))
	local := newFakeModelServer(t, []string{"local-model"}, echoReply("should not be used"))

	c := NewModelClient(ModelConfig{BaseURL: api.URL + "/v1", Name: "hosted-model", APIKey: "secret",
		probes: []localProbe{ollamaProbe(local)}, probesSet: true})
	got, err := c.Chat(context.Background(), []ChatMessage{{Role: "user", Content: "hi"}})
	if err != nil {
		t.Fatalf("Chat: %v", err)
	}
	if got != "hello from the configured model" {
		t.Fatalf("reply = %q", got)
	}
	if api.models_[0] != "hosted-model" || api.auth[0] != "Bearer secret" {
		t.Fatalf("request model %q auth %q", api.models_[0], api.auth[0])
	}
	if local.chatCount() != 0 {
		t.Fatal("local server was used despite explicit configuration")
	}
}

func TestExplicitBaseWithoutNameUsesListedModel(t *testing.T) {
	api := newFakeModelServer(t, []string{"listed-a", "listed-b"}, echoReply("ok"))
	c := NewModelClient(ModelConfig{BaseURL: api.URL + "/v1", probesSet: true})
	base, model, err := c.Resolve(context.Background())
	if err != nil || base != api.URL+"/v1" || model != "listed-a" {
		t.Fatalf("Resolve = %q %q %v", base, model, err)
	}
}

func TestLocalOllamaServerDiscoveredAndVerified(t *testing.T) {
	ollama := newFakeModelServer(t, []string{"llama3.2:latest", "qwen2.5:7b"}, echoReply("local words"))
	c := NewModelClient(ModelConfig{probes: []localProbe{ollamaProbe(ollama)}, probesSet: true})

	got, err := c.Chat(context.Background(), []ChatMessage{{Role: "user", Content: "hi"}})
	if err != nil {
		t.Fatalf("Chat: %v", err)
	}
	if got != "local words" {
		t.Fatalf("reply = %q", got)
	}
	base, model, ok := c.Resolved()
	if !ok || base != ollama.URL+"/v1" || model != "llama3.2:latest" {
		t.Fatalf("resolved %q %q %v", base, model, ok)
	}
	if ollama.auth[0] != "" {
		t.Fatalf("local request carried an API key: %q", ollama.auth[0])
	}
}

func TestLocalNamedModelSelected(t *testing.T) {
	ollama := newFakeModelServer(t, []string{"llama3.2:latest", "qwen2.5"}, echoReply("ok"))
	c := NewModelClient(ModelConfig{Name: "qwen2.5", probes: []localProbe{ollamaProbe(ollama)}, probesSet: true})
	if _, model, err := c.Resolve(context.Background()); err != nil || model != "qwen2.5" {
		t.Fatalf("Resolve model %q err %v", model, err)
	}

	c = NewModelClient(ModelConfig{Name: "llama3.2", probes: []localProbe{ollamaProbe(ollama)}, probesSet: true})
	if _, model, err := c.Resolve(context.Background()); err != nil || model != "llama3.2:latest" {
		t.Fatalf("Resolve model %q err %v", model, err)
	}

	c = NewModelClient(ModelConfig{Name: "not-pulled", probes: []localProbe{ollamaProbe(ollama)}, probesSet: true})
	if _, _, err := c.Resolve(context.Background()); err == nil || !strings.Contains(err.Error(), `"not-pulled" not served`) {
		t.Fatalf("expected not-served error, got %v", err)
	}
}

func TestLocalURLsProbedFirst(t *testing.T) {
	first := newFakeModelServer(t, []string{"from-env-url"}, echoReply("ok"))
	second := newFakeModelServer(t, []string{"from-default"}, echoReply("ok"))
	c := NewModelClient(ModelConfig{LocalURLs: []string{first.URL + "/v1/"},
		probes: []localProbe{ollamaProbe(second)}, probesSet: true})
	if _, model, err := c.Resolve(context.Background()); err != nil || model != "from-env-url" {
		t.Fatalf("Resolve model %q err %v", model, err)
	}
}

func TestServerWithEmptyModelListIsRejected(t *testing.T) {
	empty := newFakeModelServer(t, []string{}, echoReply("must not be used"))
	c := NewModelClient(ModelConfig{probes: []localProbe{ollamaProbe(empty), openAIProbe(empty)}, probesSet: true})
	_, err := c.Chat(context.Background(), []ChatMessage{{Role: "user", Content: "hi"}})
	if err == nil || !strings.Contains(err.Error(), "serves no model") {
		t.Fatalf("expected rejection of a server without models, got %v", err)
	}
	if empty.chatCount() != 0 {
		t.Fatal("chat was sent to an unverified server")
	}

	// A verified server after the empty one is used.
	good := newFakeModelServer(t, []string{"m"}, echoReply("ok"))
	c = NewModelClient(ModelConfig{probes: []localProbe{ollamaProbe(empty), openAIProbe(good)}, probesSet: true})
	if _, model, err := c.Resolve(context.Background()); err != nil || model != "m" {
		t.Fatalf("Resolve model %q err %v", model, err)
	}
}

func TestNothingAvailableIsAnError(t *testing.T) {
	gone := newFakeModelServer(t, []string{"m"}, echoReply("x"))
	gone.Close()
	c := NewModelClient(ModelConfig{probes: []localProbe{ollamaProbe(gone)}, probesSet: true})
	_, err := c.Chat(context.Background(), []ChatMessage{{Role: "user", Content: "hi"}})
	if err == nil || !strings.Contains(err.Error(), "ECHO_MODEL_BASE_URL") || !strings.Contains(err.Error(), "ollama serve") {
		t.Fatalf("expected an explanatory error, got %v", err)
	}

	var nilClient *ModelClient
	if _, err := nilClient.Chat(context.Background(), nil); err == nil {
		t.Fatal("nil client must return an error")
	}
}

func TestReResolveAfterConnectionFailure(t *testing.T) {
	first := newFakeModelServer(t, []string{"first"}, echoReply("from first"))
	second := newFakeModelServer(t, []string{"second"}, echoReply("from second"))
	c := NewModelClient(ModelConfig{probes: []localProbe{openAIProbe(first), openAIProbe(second)}, probesSet: true})
	if got, err := c.Chat(context.Background(), nil); err != nil || got != "from first" {
		t.Fatalf("first chat %q %v", got, err)
	}
	first.Close()
	if _, err := c.Chat(context.Background(), nil); err == nil {
		t.Fatal("expected a connection failure")
	}
	if _, _, ok := c.Resolved(); ok {
		t.Fatal("resolution should be forgotten after a connection failure")
	}
	if got, err := c.Chat(context.Background(), nil); err != nil || got != "from second" {
		t.Fatalf("after re-resolve %q %v", got, err)
	}
}

func TestFillPrompt(t *testing.T) {
	got := FillPrompt("explore {topic} at {strength} ({topic})", map[string]string{"topic": "fractals", "strength": "0.60"})
	if got != "explore fractals at 0.60 (fractals)" {
		t.Fatalf("FillPrompt = %q", got)
	}
}

func newTestPlaymate(t *testing.T, reply func(string) string) (*Playmate, *fakeModelServer) {
	t.Helper()
	srv := newFakeModelServer(t, []string{"m"}, reply)
	cfg := DefaultPlaymateConfig()
	cfg.Model = NewModelClient(ModelConfig{probes: []localProbe{ollamaProbe(srv)}, probesSet: true})
	p, err := NewPlaymate(cfg)
	if err != nil {
		t.Fatal(err)
	}
	return p, srv
}

func TestThoughtTemplatesFilledFromModelReplies(t *testing.T) {
	n := 0
	p, srv := newTestPlaymate(t, func(prompt string) string {
		n++
		switch {
		case strings.Contains(prompt, "open wonder"):
			return `{"thought": "model thought of wonder", "mood": 0.9, "wonder": null}`
		case strings.Contains(prompt, `interest in "fractals"`):
			return "```json\n{\"thought\": \"model thought about fractals\", \"mood\": 0.4, " +
				"\"wonder\": {\"description\": \"self-similarity\", \"reflection\": \"model reflection\", \"intensity\": 0.5}}\n```"
		case strings.Contains(prompt, `skill "juggling"`):
			return `{"thought": "model thought about juggling", "mood": 0.2}`
		}
		return `{"thought": ""}`
	})
	p.LearnInterest(InterestKnowledge, "fractals", []string{"geometry", "recursion"})
	p.LearnInterest(InterestKnowledge, "fractals", nil) // strength 0.6 > 0.5
	p.PracticeSkill("juggling", "keeping three balls in the air")

	want := []string{"model thought of wonder", "model thought about fractals", "model thought about juggling"}
	for i, w := range want {
		got, err := p.Think(context.Background())
		if err != nil {
			t.Fatalf("Think %d: %v (prompt %q)", i, err, srv.lastPrompt())
		}
		if got != w {
			t.Fatalf("Think %d = %q, want %q", i, got, w)
		}
	}
	if !strings.Contains(srv.prompts[1], "geometry, recursion") || !strings.Contains(srv.prompts[2], "proficiency 0.10") {
		t.Fatalf("slots not filled: %q / %q", srv.prompts[1], srv.prompts[2])
	}
	if strings.Contains(strings.Join(srv.prompts, ""), "{state}") {
		t.Fatal("unfilled slot sent to the model")
	}
	if p.Mood != 0.2 {
		t.Fatalf("mood = %v, want the model's 0.2", p.Mood)
	}
	if len(p.Wonders) != 1 || p.Wonders[0].Description != "self-similarity" || p.Wonders[0].Reflection != "model reflection" {
		t.Fatalf("wonders = %+v", p.Wonders)
	}
}

func TestWakeWonderAndDiscussionInsightFromModel(t *testing.T) {
	p, srv := newTestPlaymate(t, func(prompt string) string {
		switch {
		case strings.Contains(prompt, "You have just woken"):
			return `{"description": "model dawn", "reflection": "model wake reflection", "intensity": 0.6}`
		case strings.Contains(prompt, `discussion about "tides"`):
			return `{"insight": "model insight about tides", "mood": "calm"}`
		}
		return "{}"
	})
	w, err := p.generateWakeWonder(context.Background())
	if err != nil || w.Description != "model dawn" || w.Reflection != "model wake reflection" {
		t.Fatalf("wake wonder %+v %v", w, err)
	}
	if !strings.Contains(srv.lastPrompt(), "22:00 to 06:00") {
		t.Fatalf("wake prompt slots not filled: %q", srv.lastPrompt())
	}

	d := p.StartDiscussion("tides", "Ada")
	for i := 0; i < 6; i++ {
		if err := p.AddMessage(d.ID, "Ada", "the moon pulls the sea"); err != nil {
			t.Fatal(err)
		}
	}
	if err := p.EndDiscussion(context.Background(), d.ID); err != nil {
		t.Fatalf("EndDiscussion: %v", err)
	}
	if len(d.Insights) != 1 || d.Insights[0] != "model insight about tides" || d.Mood != "calm" || p.TotalInsights != 1 {
		t.Fatalf("discussion = %+v", d)
	}
	if !strings.Contains(srv.lastPrompt(), "with Ada") || !strings.Contains(srv.lastPrompt(), "Ada: the moon pulls the sea") {
		t.Fatalf("insight prompt slots not filled: %q", srv.lastPrompt())
	}
}

func TestNoModelProducesNoThought(t *testing.T) {
	cfg := DefaultPlaymateConfig()
	cfg.Model = NewModelClient(ModelConfig{probesSet: true}) // nothing configured, nothing served
	p, err := NewPlaymate(cfg)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := p.Think(context.Background()); err == nil {
		t.Fatal("expected an error without a model")
	}
	if len(p.StreamOfThoughts) != 0 || p.Mood != 0.5 {
		t.Fatalf("state changed without a model: thoughts %v mood %v", p.StreamOfThoughts, p.Mood)
	}
}

func TestWisdomFoundationalPrinciplesSeededByModel(t *testing.T) {
	srv := newFakeModelServer(t, []string{"m"}, func(prompt string) string {
		return `{"principles": [
			{"statement": "model principle one", "dimensions": ["understanding", "reflection"]},
			{"statement": "model principle two", "dimensions": ["Perspective", "bogus"]},
			{"statement": "  ", "dimensions": []}]}`
	})
	model := NewModelClient(ModelConfig{probes: []localProbe{openAIProbe(srv)}, probesSet: true})
	wc, err := NewWisdomCultivator(&WisdomConfig{Model: model})
	if err != nil {
		t.Fatal(err)
	}
	ps := wc.GetPrinciples()
	if len(ps) != 2 {
		t.Fatalf("principles = %d, want 2", len(ps))
	}
	got := map[string][]WisdomDimension{}
	for _, p := range ps {
		got[p.Statement] = p.Dimensions
		if p.Source != "foundational" {
			t.Fatalf("source = %q", p.Source)
		}
	}
	if d := got["model principle two"]; len(d) != 1 || d[0] != DimensionPerspective {
		t.Fatalf("dimensions = %v", d)
	}
	if !strings.Contains(srv.lastPrompt(), "1. understanding and reflection") {
		t.Fatalf("dimension pairs not in prompt: %q", srv.lastPrompt())
	}

	// Without a model nothing is seeded.
	wc, err = NewWisdomCultivator(&WisdomConfig{Model: NewModelClient(ModelConfig{probesSet: true})})
	if err != nil || len(wc.GetPrinciples()) != 0 {
		t.Fatalf("expected no principles without a model, got %d (%v)", len(wc.GetPrinciples()), err)
	}
}
