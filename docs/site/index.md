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
  <strong>For everyone:</strong>
  <a href="{{ '/guide/' | relative_url }}">User guide</a>
  &middot;
  <a href="{{ '/guide/glossary/' | relative_url }}">Glossary</a>
  <br>
  <strong>For developers:</strong>
  <a href="{{ '/api/' | relative_url }}">Studio GUI API</a>
  &middot;
  <a href="https://bamath.org/libanpcpp/">ANP library (libanpcpp)</a>
</p>

{% include_relative README.content.md %}
