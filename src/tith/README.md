Goals:
- Simple to implement
- Easily extensible
- Use public key authentication
- Use readily available algoithms
- Always authenticate the server
- Allow anonymous sends (for new nodes)
- Allow replies to anonymous sends (for new nodes)
	- Require daily poll to keep "alive"
- Do not enforce any arbitrary limits
- No legacy baggage

Non-goals:
- Password authentication
- Abort/Resume
- Allowing multiple addresses on either end of a connection
- Optional behaviour (enecryption, authentication, etc)
- Compatibility
