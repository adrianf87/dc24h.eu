# Optimization — alpha-0.9.14

C++20 memory and routing. The branch starts from main `f7e465c`. The branch name is `alpha-0.9.14`.

Routed publications keep the same ADC bytes. Two local costs change. There is no new route, INI key, schema migration, Apache setting, CLI flag, WebAdmin page or status field. This is not a capacity claim or a load test.

## Shared frames

Public chat, private chat, search, results, connect-to-me, reverse connect, hub messages and routed status each build one immutable frame. The frame and its `std::shared_ptr` control block now come from one `std::make_shared` allocation. A passkey keeps construction with the router that already owned it. Dispatch still hands every recipient the same frame. A zero allocation budget during dispatch still means the fanout does not allocate another frame.

## Welcome list

Preparing the initial user list already encodes each frame to measure the output budget. That encoding stays with the plan. Login queues those bytes and does not serialize the same messages again. The stored message remains a copy, so a later rename of the source session does not change the plan. A fresh serialize of the stored message matches the retained wire.

## Limits

The wire format, recipient set, output quotas and refusal errors stay. `configuration.manage` and `hub.manage` stay unimplemented. Browser kick and mute are still not dispatched. Schema v9 is unchanged. `Require local` stays. Status and health stay bearer-only. This does not close a residual threat.
