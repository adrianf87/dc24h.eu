# WebAdmin (planned)

The final stack is React + TypeScript + Vite, served by Apache2 with reverse proxy requests to a secured C++ API. alpha-0.1.7 reserves `webadmin/` but includes no frontend application, npm dependencies, build output or HTTP endpoints. Apache2 is not needed to compile or test the current transport milestones.

Implement the UI after defining real API contracts for authentication, roles and administrative actions. It must display actual server state and enforce authorization on the server. Database credentials must never be shipped to the frontend. HTTP TLS/reverse proxy configuration is distinct from the native ADCS listener; Apache2 is not used to terminate the tested ADC/ADCS sockets.
