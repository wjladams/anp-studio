---
layout: page
title: Structure
permalink: /guide/structure/
---

The **Structure** stage is where you build the model: networks, clusters, nodes, and (later) connections.

## Networks and subnetworks

- The file you open or create is a **document** whose top level is the root network.  
- Some nodes can own a **subnetwork** (a nested model). Use the breadcrumb or network path control to move up and down.  
- **Network → Root Network** returns to the top; **Up Subnetwork** goes one level up.

Try **File → Open Sample… → `02_ahp_best_car.anpstudio`** for a small hierarchy with no nesting, or a BCR/BOCR sample for control nodes with subnetworks.

## Clusters and nodes

- A **cluster** is a group (e.g. Criteria, Alternatives).  
- A **node** is one element inside a cluster.  
- One cluster is often marked as the **alternatives** cluster (Inspector on a cluster → **Set as alternatives cluster**).

Typical edits (Structure canvas and menus):

- Add clusters and nodes from the canvas / Network workflow (context menus and Inspector).  
- Rename and describe them in the **Inspector** (right side): Name and Description.  
- **Network → Organize Clusters** lays out clusters on the canvas.

## Inspector (network selected)

Click empty canvas or clear selection so the Inspector shows **Network**. Useful fields:

- **Formula** — how alternative scores combine when there are subnetworks: Additive, Multiplicative, or Custom expression (e.g. `Benefits / Costs`).  
- **Limit matrix** — method used for limit / globals / sensitivity defaults: Calculus, New Hierarchy, or Sinks (plus method-specific flags).  
- **Subnetworks** tree — jump into nested models.

These settings are stored with the network in the JSON file.

## Tips

- Keep alternatives in a clearly named cluster.  
- Use descriptions for yourself and collaborators; they travel with the file.  
- After structure is stable, move to [Connections]({{ '/guide/connections/' | relative_url }}) before entering many judgments.

Next: [Connections]({{ '/guide/connections/' | relative_url }}) · Back to [User guide]({{ '/guide/' | relative_url }})
