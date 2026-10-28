// Package playmate - model.go is the single path by which the playmate produces language.
//
// Every thought, wonder, insight, mood change and principle presented as the playmate's own
// comes from a model through ModelClient. It speaks the OpenAI-compatible chat-completions
// protocol (POST {base}/chat/completions), which hosted APIs and local servers (Ollama,
// llama.cpp server, LM Studio, vLLM, text-generation-webui) all serve.
//
// Model resolution, in order:
//  1. Explicit configuration: ECHO_MODEL_BASE_URL (e.g. https://api.openai.com/v1 or
//     http://127.0.0.1:11434/v1), optional ECHO_MODEL_NAME, optional ECHO_MODEL_API_KEY.
//     With a base URL but no name, the endpoint's /models listing must name a model.
//  2. Otherwise a locally served model, verified before use: the server must answer AND list
//     at least one model. Probed in order: ECHO_MODEL_LOCAL_URLS (comma-separated base URLs,
//     {base}/models), Ollama (127.0.0.1:11434/api/tags), then OpenAI-compatible /v1/models on
//     ports 8080 (llama.cpp), 1234 (LM Studio), 8000 (vLLM) and 5000 (text-generation-webui).
//     When ECHO_MODEL_NAME is set, a server must serve that model; otherwise the first listed
//     model is used.
//  3. Nothing configured and no local model served: an error explaining how to fix it.
//
// The resolution is cached and redone after a connection failure. There is no offline, mock
// or fallback mode: on failure the caller gets an error and produces no character output.
package playmate

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"
)

// ChatMessage is one message of a chat-completions request.
type ChatMessage struct {
	Role    string `json:"role"`
	Content string `json:"content"`
}

// localProbe is one candidate local model server.
type localProbe struct {
	listURL    string // URL to GET
	baseURL    string // chat base URL when the probe verifies a model
	ollamaTags bool   // listing format: Ollama /api/tags ("models"[].name) vs OpenAI /models ("data"[].id)
}

// ModelConfig configures a ModelClient. NewModelClientFromEnv fills it from the environment.
type ModelConfig struct {
	BaseURL   string   // explicit OpenAI-compatible base URL; empty = discover a local model
	Name      string   // model name; empty = first model the endpoint lists
	APIKey    string   // bearer token; local servers need none
	LocalURLs []string // extra local base URLs probed first

	Timeout      time.Duration // chat request timeout (default 60 s)
	ProbeTimeout time.Duration // per-probe timeout for local discovery (default 1.5 s)
	HTTPClient   *http.Client  // optional; default http.Client

	// probes replaces the built-in local candidates (Ollama, 8080, 1234, 8000, 5000) when
	// probesSet is true; used by tests so they never depend on what runs on this machine.
	probes    []localProbe
	probesSet bool
}

// ModelClient sends chat requests to the resolved model.
type ModelClient struct {
	cfg ModelConfig

	mu       sync.Mutex
	resolved bool
	baseURL  string
	model    string
}

// NewModelClient creates a client; the model is resolved lazily on first use.
func NewModelClient(cfg ModelConfig) *ModelClient {
	if cfg.Timeout <= 0 {
		cfg.Timeout = 60 * time.Second
	}
	if cfg.ProbeTimeout <= 0 {
		cfg.ProbeTimeout = 1500 * time.Millisecond
	}
	if cfg.HTTPClient == nil {
		cfg.HTTPClient = &http.Client{}
	}
	cfg.BaseURL = strings.TrimRight(strings.TrimSpace(cfg.BaseURL), "/")
	cfg.Name = strings.TrimSpace(cfg.Name)
	return &ModelClient{cfg: cfg}
}

// NewModelClientFromEnv creates a client from ECHO_MODEL_BASE_URL / ECHO_MODEL_NAME /
// ECHO_MODEL_API_KEY / ECHO_MODEL_LOCAL_URLS. It does not contact any server; the model is
// resolved (explicitly or by verified local discovery) on first use.
func NewModelClientFromEnv() (*ModelClient, error) {
	cfg := ModelConfig{
		BaseURL: os.Getenv("ECHO_MODEL_BASE_URL"),
		Name:    os.Getenv("ECHO_MODEL_NAME"),
		APIKey:  os.Getenv("ECHO_MODEL_API_KEY"),
	}
	if base := strings.TrimSpace(cfg.BaseURL); base != "" &&
		!strings.HasPrefix(base, "http://") && !strings.HasPrefix(base, "https://") {
		return nil, fmt.Errorf("ECHO_MODEL_BASE_URL must be an http(s) URL, got %q", base)
	}
	for _, u := range strings.Split(os.Getenv("ECHO_MODEL_LOCAL_URLS"), ",") {
		if u = strings.TrimSpace(u); u != "" {
			cfg.LocalURLs = append(cfg.LocalURLs, u)
		}
	}
	return NewModelClient(cfg), nil
}

func defaultLocalProbes() []localProbe {
	probes := []localProbe{{
		listURL:    "http://127.0.0.1:11434/api/tags",
		baseURL:    "http://127.0.0.1:11434/v1",
		ollamaTags: true,
	}}
	for _, port := range []int{8080, 1234, 8000, 5000} {
		base := fmt.Sprintf("http://127.0.0.1:%d/v1", port)
		probes = append(probes, localProbe{listURL: base + "/models", baseURL: base})
	}
	return probes
}

// Resolved reports the cached endpoint and model, if any.
func (c *ModelClient) Resolved() (baseURL, model string, ok bool) {
	if c == nil {
		return "", "", false
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.baseURL, c.model, c.resolved
}

// Resolve determines the endpoint and model (cached until a connection failure).
func (c *ModelClient) Resolve(ctx context.Context) (baseURL, model string, err error) {
	if c == nil {
		return "", "", errors.New("no model client: set ECHO_MODEL_BASE_URL or serve a local model (e.g. `ollama serve` and `ollama pull <model>`)")
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.resolved {
		return c.baseURL, c.model, nil
	}

	base, name, err := c.resolveLocked(ctx)
	if err != nil {
		return "", "", err
	}
	c.resolved, c.baseURL, c.model = true, base, name
	return base, name, nil
}

func (c *ModelClient) resolveLocked(ctx context.Context) (string, string, error) {
	cfg := c.cfg
	if cfg.BaseURL != "" {
		// A hosted API may not list its models: an explicit endpoint plus name is used as given.
		if cfg.Name != "" {
			return cfg.BaseURL, cfg.Name, nil
		}
		ids, err := c.listModels(ctx, localProbe{listURL: cfg.BaseURL + "/models", baseURL: cfg.BaseURL}, cfg.Timeout, true)
		if err != nil {
			return "", "", fmt.Errorf("no model available at %s (%v); set ECHO_MODEL_NAME or check the endpoint", cfg.BaseURL, err)
		}
		return cfg.BaseURL, ids[0], nil
	}

	var probes []localProbe
	for _, u := range cfg.LocalURLs {
		u = strings.TrimRight(strings.TrimSpace(u), "/")
		if u != "" {
			probes = append(probes, localProbe{listURL: u + "/models", baseURL: u})
		}
	}
	if cfg.probesSet {
		probes = append(probes, cfg.probes...)
	} else {
		probes = append(probes, defaultLocalProbes()...)
	}

	tried := make([]string, 0, len(probes))
	for _, p := range probes {
		ids, err := c.listModels(ctx, p, cfg.ProbeTimeout, false)
		if err != nil {
			tried = append(tried, fmt.Sprintf("%s: %v", p.listURL, err))
			continue
		}
		if cfg.Name == "" {
			return p.baseURL, ids[0], nil
		}
		for _, id := range ids {
			if id == cfg.Name || id == cfg.Name+":latest" {
				return p.baseURL, id, nil
			}
		}
		tried = append(tried, fmt.Sprintf("%s: model %q not served", p.listURL, cfg.Name))
	}
	return "", "", fmt.Errorf("no model available: ECHO_MODEL_BASE_URL is not set and no local model server is serving a model "+
		"(tried %s). Serve one locally, e.g. `ollama serve` and `ollama pull <model>`, or set ECHO_MODEL_BASE_URL",
		strings.Join(tried, "; "))
}

// listModels GETs a model listing and returns the model ids; a server that answers but lists
// no model is an error (it is not verified).
func (c *ModelClient) listModels(ctx context.Context, p localProbe, timeout time.Duration, auth bool) ([]string, error) {
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, p.listURL, nil)
	if err != nil {
		return nil, err
	}
	if auth && c.cfg.APIKey != "" {
		req.Header.Set("Authorization", "Bearer "+c.cfg.APIKey)
	}
	resp, err := c.cfg.HTTPClient.Do(req)
	if err != nil {
		return nil, errors.New("no server")
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("HTTP %d", resp.StatusCode)
	}
	var listing struct {
		Models []struct {
			Name string `json:"name"`
		} `json:"models"`
		Data []struct {
			ID string `json:"id"`
		} `json:"data"`
	}
	if err := json.NewDecoder(io.LimitReader(resp.Body, 4<<20)).Decode(&listing); err != nil {
		return nil, fmt.Errorf("unreadable model listing: %v", err)
	}
	var ids []string
	if p.ollamaTags {
		for _, m := range listing.Models {
			if m.Name != "" {
				ids = append(ids, m.Name)
			}
		}
	} else {
		for _, m := range listing.Data {
			if m.ID != "" {
				ids = append(ids, m.ID)
			}
		}
	}
	if len(ids) == 0 {
		return nil, errors.New("serves no model")
	}
	return ids, nil
}

// invalidate forgets the cached resolution so the next request resolves again.
func (c *ModelClient) invalidate() {
	c.mu.Lock()
	c.resolved = false
	c.mu.Unlock()
}

// Chat sends a chat request and returns the model's reply text.
func (c *ModelClient) Chat(ctx context.Context, messages []ChatMessage) (string, error) {
	base, model, err := c.Resolve(ctx)
	if err != nil {
		return "", err
	}

	payload, err := json.Marshal(map[string]interface{}{
		"model":       model,
		"messages":    messages,
		"temperature": 0.8,
	})
	if err != nil {
		return "", err
	}
	reqCtx, cancel := context.WithTimeout(ctx, c.cfg.Timeout)
	defer cancel()
	req, err := http.NewRequestWithContext(reqCtx, http.MethodPost, base+"/chat/completions", bytes.NewReader(payload))
	if err != nil {
		return "", err
	}
	req.Header.Set("Content-Type", "application/json")
	if c.cfg.APIKey != "" && c.cfg.BaseURL != "" {
		req.Header.Set("Authorization", "Bearer "+c.cfg.APIKey)
	}

	resp, err := c.cfg.HTTPClient.Do(req)
	if err != nil {
		// The server went away: resolve again next time (a local model may have moved).
		c.invalidate()
		return "", fmt.Errorf("model request to %s failed: %w", base, err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(resp.Body, 8<<20))
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		snippet := string(body)
		if len(snippet) > 512 {
			snippet = snippet[:512]
		}
		return "", fmt.Errorf("model request failed: HTTP %d: %s", resp.StatusCode, snippet)
	}
	var out struct {
		Choices []struct {
			Message struct {
				Content string `json:"content"`
			} `json:"message"`
		} `json:"choices"`
	}
	if err := json.Unmarshal(body, &out); err != nil {
		return "", fmt.Errorf("model response unreadable: %w", err)
	}
	if len(out.Choices) == 0 {
		return "", errors.New("model response has no choices")
	}
	text := strings.TrimSpace(out.Choices[0].Message.Content)
	if text == "" {
		return "", errors.New("model response has empty message content")
	}
	return text, nil
}

// ChatJSON sends a chat request and decodes the first JSON value (object or array) in the
// reply into v (models sometimes wrap JSON in prose or code fences).
func (c *ModelClient) ChatJSON(ctx context.Context, messages []ChatMessage, v interface{}) error {
	text, err := c.Chat(ctx, messages)
	if err != nil {
		return err
	}
	start := strings.IndexAny(text, "{[")
	if start < 0 {
		return fmt.Errorf("model reply is not JSON: %q", truncate(text, 200))
	}
	closer := byte('}')
	if text[start] == '[' {
		closer = ']'
	}
	end := strings.LastIndexByte(text, closer)
	if end <= start {
		return fmt.Errorf("model reply is not JSON: %q", truncate(text, 200))
	}
	if err := json.Unmarshal([]byte(text[start:end+1]), v); err != nil {
		return fmt.Errorf("model reply JSON invalid: %v", err)
	}
	return nil
}

func truncate(s string, n int) string {
	if len(s) <= n {
		return s
	}
	return s[:n] + "..."
}
