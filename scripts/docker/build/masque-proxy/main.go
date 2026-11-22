// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved.
//
// A minimal MASQUE CONNECT-UDP proxy (RFC 9298) for the tsf-masque test
// suite, built on quic-go's masque-go (v0.6.0 API). It terminates HTTP/3
// on -addr and answers the CONNECT-UDP template
//
//	https://<addr>/.well-known/masque/udp/{target_host}/{target_port}/
//
// forwarding to the requested UDP target. The template host is taken from
// -addr so it matches the :authority the suite's clients send.
//
// connect-tcp is NOT served; such a request gets the default 404, which
// the suite reads as "no connect-tcp" and skips.
package main

import (
	"errors"
	"flag"
	"log"
	"net/http"

	"github.com/quic-go/masque-go"
	"github.com/quic-go/quic-go/http3"
	"github.com/yosida95/uritemplate/v3"
)

func main() {
	addr := flag.String("addr", "0.0.0.0:4433", "UDP listen address")
	cert := flag.String("cert", "/tmp/masque-cert.pem", "TLS certificate")
	key := flag.String("key", "/tmp/masque-key.pem", "TLS private key")
	flag.Parse()

	// The template host must equal the :authority the client sends, which
	// is the proxy's own -addr.
	tmpl := uritemplate.MustNew(
		"https://" + *addr +
			"/.well-known/masque/udp/{target_host}/{target_port}/")

	var proxy masque.Proxy
	defer proxy.Close()

	http.HandleFunc("/.well-known/masque/udp/",
		func(w http.ResponseWriter, r *http.Request) {
			preq, err := masque.ParseProxyRequest(r, tmpl)
			if err != nil {
				var perr *masque.ProxyRequestParseError
				if errors.As(err, &perr) {
					w.WriteHeader(perr.HTTPStatus)
					return
				}
				w.WriteHeader(http.StatusBadRequest)
				return
			}
			if err := proxy.Proxy(w, preq); err != nil {
				log.Printf("masque-proxy: proxying %s: %v",
					preq.Target, err)
			}
		})

	server := http3.Server{Addr: *addr, EnableDatagrams: true}
	log.Printf("masque-proxy: CONNECT-UDP on %s", *addr)
	log.Fatal(server.ListenAndServeTLS(*cert, *key))
}
