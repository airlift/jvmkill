import java.util.ArrayList;
import java.util.List;

public final class JvmKillTest
{
    @SuppressWarnings("InfiniteLoopStatement")
    public static void main(String[] args)
            throws Exception
    {
        System.out.println("triggering OutOfMemmory...");
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
