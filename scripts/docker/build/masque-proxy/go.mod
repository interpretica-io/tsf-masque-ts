module masque-proxy

go 1.23

// Only masque-go is pinned; `go mod tidy` at image build time resolves
// quic-go, uritemplate and the rest at the versions it requires.
require github.com/quic-go/masque-go v0.6.0
