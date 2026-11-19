import java.util.Arrays;
import javax.tools.JavaCompiler;
import javax.tools.StandardJavaFileManager;
import javax.tools.ToolProvider;

public class Compilar {
    public static void main(String[] a) {
        JavaCompiler c = ToolProvider.getSystemJavaCompiler();
        StandardJavaFileManager f = c.getStandardFileManager(null, null, null);
        boolean ok = c.getTask(null, f, null, Arrays.asList("-source", "17", "-target", "17", "-Xlint:-options", "-cp", a[0], "-d", a[1]), null, f.getJavaFileObjects(a[2])).call();
        // ZipFS.close pide toRealPath fuera del sandbox; el proceso libera estos archivos al terminar.
        System.exit(ok ? 0 : 1);
    }
}
