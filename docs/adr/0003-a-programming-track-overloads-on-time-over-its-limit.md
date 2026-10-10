# A programming track overloads on time over its limit

NMRA S-9.2.3 defines the limited energy of service mode as 250 mA sustained
for more than 100 ms. An ack is at least 60 mA for 6 ms, with no upper bound.

On rails49 a Lenz Silver+ mini acks with a train of 6.5 ms pulses at 460-500
mA, about 7 ms apart, for 75-100 ms. Two rules cut it at a 250 mA limit:

- The fast overcurrent guard cut a track after 1 ms over its limit.
- The main loop cuts a track 100 ms after it first went over its limit, unless
  20 ms under the limit came in between. The gaps in the pulse train are
  shorter than that, so the train counted as one 100 ms overload.

Upstream avoids both by lifting the Prog Limit during ack operations. A limit
set with `<JG>` is not lifted (ADR-0002).

On a PROG track that is not joined:

- The fast guard watches the fault pin only, not the current.
- The main loop cuts the track once the time over its limit in one ALERT
  period adds up to 100 ms. The 20 ms that end an ALERT period are unchanged.

## Consequences

- A train of short pulses over the limit is cut only when the pulses add up to
  100 ms. A steady overload is cut after 100 ms, as before.
- A fault that pulses with gaps shorter than 20 ms runs longer before it is
  cut than it did, by the time spent in the gaps.
- Tracks in any other mode, and a joined PROG track, are unchanged.
