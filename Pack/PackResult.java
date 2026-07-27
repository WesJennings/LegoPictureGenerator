import java.util.ArrayList;
import java.util.List;

/** Result of a packing run for comparison. */
public class PackResult {
  public final String modeName;
  public final List<PlacedPart> placed;
  public final long elapsedMs;
  public final String status;

  public PackResult(String modeName, List<PlacedPart> placed, long elapsedMs, String status) {
    this.modeName = modeName;
    this.placed = placed != null ? placed : new ArrayList<>();
    this.elapsedMs = elapsedMs;
    this.status = status != null ? status : "ok";
  }

  public int pieceCount() {
    return placed.size();
  }
}
