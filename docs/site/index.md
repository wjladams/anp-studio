---
layout: default
title: ANP Studio
permalink: /
---

<div class="brand-lockup">
  <img class="brand-lockup__icon"
       src="{{ "/assets/anpstudio.png" | relative_url }}"
       width="64" height="64"
       alt="ANP Studio">
  <div class="brand-lockup__text">
    <h1>ANP Studio</h1>
    <p class="tagline">Analytic Network Process modeling for research and education</p>
  </div>
</div>

<p class="docs-nav">
  <strong>Documentation:</strong>
  <a href="{{ '/api/' | relative_url }}">GUI API reference</a>
  &middot;
  <a href="https://bamath.org/libanpcpp/api/examples.html">Library examples</a>
</p>

{% include_relative README.content.md %}
