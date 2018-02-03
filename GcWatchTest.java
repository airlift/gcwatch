/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ThreadLocalRandom;

// run with these JVM arguments: -XX:+UseG1GC -Xmx1g -Xms1g
public class GcWatchTest
{
    private static final List<byte[]> VALUES = new ArrayList<>();
    private static final int VALUE_SIZE = 100 * 1024;
    private static final long MAX_MEMORY_SIZE = (long) (0.9 * 1024 * 1024 * 1024);
    private static long memorySize;

    public static void main(String[] args)
            throws InterruptedException
    {
        while (true) {
            byte[] value = new byte[VALUE_SIZE];
            if (memorySize > MAX_MEMORY_SIZE) {
                VALUES.set(ThreadLocalRandom.current().nextInt(VALUES.size()), value);
            }
            else {
                VALUES.add(value);
                memorySize += VALUE_SIZE;
            }
            Thread.sleep(1);
        }
    }
}
