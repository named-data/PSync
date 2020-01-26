PSync version 0.2.0
-------------------

**New features**:

- **breaking change** Compress Sync Data in Full Sync by default (:issue:`5061`, :issue:`4917`)
- Support various compression schemes for compressing IBF and Sync Data (:issue:`5061`)

**Changes**

- Use constant interest timeout for (discovery) sync Interest in Segment Fetcher (:issue:`4945`)
- Make default jitter shorter, react to NACK after jitter
- Use boost::bimap instead of two std::map variables to store hash to prefix matching
