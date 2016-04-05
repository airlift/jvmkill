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

public final class JvmKillTest
{
    @SuppressWarnings("InfiniteLoopStatement")
    public static void main(String[] args)
            throws Exception
    {
        System.out.println("triggering OutOfMemory...");
        List<Object> list = new ArrayList<>();
        try {
            while (true) {
                byte[] bytes = new byte[1024 * 1024 * 1024];
                list.add(bytes);
                System.out.println("list size: " + list.size());
            }
        }
        catch (Throwable t) {
            System.out.println(t.toString());
        }
        System.out.println("final list size: " + list.size());
    }
}
