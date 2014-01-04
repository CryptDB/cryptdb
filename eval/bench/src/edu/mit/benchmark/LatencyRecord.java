package edu.mit.benchmark;

import java.util.ArrayList;
import java.util.Iterator;

/** Efficiently stores a record of (start time, latency) pairs. */
public class LatencyRecord implements Iterable<LatencyRecord.Sample> {
    /** Allocate int[] arrays of this length (262144 = 1 MB). */
    static final int BLOCK_SIZE = 262144;

    /** Contains (start time, latency) pairs in microsecond form. The start times are
    "compressed" by encoding them as increments, starting from startNs. A 32-bit integer
    provides sufficient resolution for an interval of 2146 seconds, or 35 minutes. */
    // TODO: Use a real variable length encoding?
    private final ArrayList<int[]> values = new ArrayList<int[]>();
    private int nextIndex;

    private final long startNs;
    private long lastNs;

    public LatencyRecord(long startNs) {
        assert startNs > 0;

        this.startNs = startNs;
        lastNs = startNs;
        allocateChunk();
    }

    public void addLatency(long startNs, long endNs) {
        assert lastNs > 0;
        assert lastNs - 500 <= startNs;
        assert endNs >= startNs;

        if (nextIndex == BLOCK_SIZE) {
            allocateChunk();
        }
        int[] chunk = values.get(values.size() - 1);

        int startOffsetUs = (int) ((startNs - lastNs + 500) / 1000);
        assert startOffsetUs >= 0;
        int latencyUs = (int) ((endNs - startNs + 500) / 1000);
        assert latencyUs >= 0;

        chunk[nextIndex] = startOffsetUs;
        chunk[nextIndex + 1] = latencyUs;
        nextIndex += 2;

        lastNs += startOffsetUs * 1000L;
    }

    private void allocateChunk() {
        assert (values.isEmpty() && nextIndex == 0) || nextIndex == BLOCK_SIZE;
        values.add(new int[BLOCK_SIZE]);
        nextIndex = 0;
    }

    /** Returns the number of recorded samples. */
    public int size() {
        // Samples stored in full chunks
        int samples = (values.size() - 1) * (BLOCK_SIZE / 2);

        // Samples stored in the last not full chunk
        samples += nextIndex / 2;
        return samples;
    }

    /** Stores the start time and latency for a single sample. Immutable. */
    public static final class Sample implements Comparable<Sample> {
        public final long startNs;
        public final int latencyUs;

        public Sample(long startNs, int latencyUs) {
            this.startNs = startNs;
            this.latencyUs = latencyUs;
        }

        @Override
        public int compareTo(Sample other) {
            long diff = this.startNs - other.startNs;

            // explicit comparison to avoid long to int overflow
            if (diff > 0) return 1;
            else if (diff < 0) return -1;
            else {
                assert diff == 0;
                return 0;
            }
        }
    }

    private final class LatencyRecordIterator implements Iterator<Sample> {
        private int chunkIndex = 0;
        private int subIndex = 0;
        private long lastIteratorNs = startNs;

        @Override
        public boolean hasNext() {
            if (chunkIndex < values.size() - 1) {
                return true;
            }

            assert chunkIndex == values.size() - 1;
            if (subIndex < nextIndex) {
                return true;
            }

            assert chunkIndex == values.size() - 1 && subIndex == nextIndex;
            return false;
        }

        @Override
        public Sample next() {
            int[] chunk = values.get(chunkIndex);
            int offsetUs = chunk[subIndex];
            int latencyUs = chunk[subIndex + 1];
            subIndex += 2;
            if (subIndex == BLOCK_SIZE) {
                chunkIndex += 1;
                subIndex = 0;
            }

            long startNs = lastIteratorNs + offsetUs * 1000L;
            lastIteratorNs = startNs;
            return new Sample(startNs, latencyUs);
        }

        @Override
        public void remove() {
            throw new UnsupportedOperationException("remove is not supported");
        }
    }

    public Iterator<Sample> iterator() {
        return new LatencyRecordIterator();
    }
}
