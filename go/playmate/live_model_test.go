package playmate

import (
	"context"
	"os"
	"testing"
	"time"
)

// TestLiveLocalModel exercises the playmate against a really served model. It runs only when
// ECHO_LIVE_MODEL_TEST=1, with no explicit endpoint required: the client must discover and
// verify the local server itself (or use ECHO_MODEL_* if set).
func TestLiveLocalModel(t *testing.T) {
	if os.Getenv("ECHO_LIVE_MODEL_TEST") != "1" {
		t.Skip("set ECHO_LIVE_MODEL_TEST=1 with a model served locally")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
	defer cancel()

	p, err := NewPlaymate(nil)
	if err != nil {
		t.Fatalf("NewPlaymate: %v", err)
	}
	base, model, err := p.model.Resolve(ctx)
	if err != nil {
		t.Fatalf("resolve: %v", err)
	}
	t.Logf("resolved %s model %s", base, model)

	thought, err := p.Think(ctx)
	if err != nil {
		t.Fatalf("Think: %v", err)
	}
	if thought == "" {
		t.Fatal("empty thought")
	}
	t.Logf("thought: %s", thought)

	w, err := p.ExperienceWonder(ctx, "a child shows Echo a tree whose branches echo its roots")
	if err != nil {
		t.Fatalf("ExperienceWonder: %v", err)
	}
	t.Logf("wonder: %s (intensity %.2f)", w.Description, w.Intensity)
}
